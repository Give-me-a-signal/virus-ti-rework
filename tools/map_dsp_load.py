#!/usr/bin/env python3
"""Map decoded TI DSP loader records offline; never execute or modify firmware."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


def map_image(directory):
    source = directory / 'dsp-command-stream24be.bin'
    data = source.read_bytes()
    if len(data) % 3:
        raise ValueError('Incomplete BE24 word')
    words = [int.from_bytes(data[i:i + 3], 'big') for i in range(0, len(data), 3)]
    manifest = json.loads((directory / 'manifest.json').read_text())
    out = directory / 'dsp-load'
    out.mkdir(exist_ok=True)
    chunks = {e['tag']: e for e in manifest['chunks']}

    def source_location(stream_word):
        remaining = stream_word + manifest['dsp_boot']['word_count'] + 2
        for block in manifest['primary_dsp_word_blocks']:
            if remaining < block['word_count']:
                bank_offset = 3 + remaining * 3
                entry = chunks[block['tag']]
                encoded_offset = bank_offset if entry['encoded_size'] == 0x8000 else (bank_offset // 32) * 35 + 3 + bank_offset % 32
                return {'tag': block['tag'], 'decoded_bank_offset': bank_offset,
                        'original_vti_image_byte_offset': entry['source_payload_offset'] + encoded_offset}
            remaining -= block['word_count']
        raise ValueError('Stream word outside source banks')

    blocks = []
    index = 0
    sine = None
    while index < len(words):
        if index + 2 > len(words):
            raise ValueError('Truncated loader command')
        command, address = words[index:index + 2]
        if command == 4:
            jump = {'stream_word_offset': index, 'destination_word_address': address,
                    'trailing_words': words[index + 2:],
                    'trailing_word_semantics': 'unresolved; preserved without interpretation'}
            break
        if command not in (0, 1, 2, 3) or index + 3 > len(words):
            raise ValueError(f'Unknown or truncated command at word {index}')
        count = words[index + 2]
        start, end = index + 3, index + 3 + count
        if end > len(words):
            raise ValueError('Truncated loader data')
        payload = words[start:end]
        space = {0: 'P', 1: 'X', 2: 'Y', 3: 'Y-split12'}[command]
        filename = f'{len(blocks):02}_{space}_{address:06x}.24be.bin'
        raw = data[start * 3:end * 3]
        (out / filename).write_bytes(raw)
        block = {'command': command, 'destination_space': space,
                 'destination_word_address': address, 'source_word_count': count,
                 'destination_word_count': count * (2 if command == 3 else 1),
                 'stream_command_word_offset': index, 'stream_data_word_offset': start,
                 'source_start': source_location(start), 'source_last_word': source_location(end - 1),
                 'file': filename, 'sha256': hashlib.sha256(raw).hexdigest()}
        blocks.append(block)
        if command == 3 and count >= 64:
            # This tests an explicit numeric hypothesis, not oscillator use.
            unsigned = [v for word in payload[:64] for v in (word >> 12, word & 0xfff)]
            signed = [v - 4096 if v >= 2048 else v for v in unsigned]
            errors = [signed[i] + 2048 * math.sin(2 * math.pi * i / 128) for i in range(128)]
            error = max(abs(v) for v in errors)
            if error <= 1.000001:
                with (out / 'sine-lookup-128-signed12.csv').open('w') as f:
                    writer = csv.writer(f)
                    writer.writerow(['index', 'signed_12_bit_value'])
                    writer.writerows(enumerate(signed))
                sine = {'destination_start_word': address, 'samples': 128,
                        'numeric_interpretation': 'two signed 12-bit values, high half then low half',
                        'min': min(signed), 'max': max(signed),
                        'max_absolute_error_against_negative_2048_sine': error,
                        'source_start': source_location(start),
                        'csv': 'sine-lookup-128-signed12.csv',
                        'limitation': 'Verified sinusoidal numeric sequence, not a verified wavetable slot or oscillator lookup reference.'}
        index = end
    else:
        raise ValueError('No terminal jump found')
    result = {'source': str(source.resolve()), 'source_sha256': hashlib.sha256(data).hexdigest(),
              'format_reference': 'https://theusualsuspects.io/technical/osirus-c',
              'validation': 'All 16 load records bounded and exported; terminal jump and trailing words retained',
              'blocks': blocks, 'terminal_jump': jump, 'sine_lookup': sine,
              'limitations': ['Destination spaces follow the published boot-loader model; no DSP execution was performed.',
                             'External X/P aliasing is not resolved here; an X load can include executable code.',
                             'No mapping from the 100 selection IDs to waveform sample addresses is established.']}
    (out / 'map.json').write_text(json.dumps(result, indent=2) + '\n')
    return {'image': directory.name, 'blocks': len(blocks), 'jump': hex(jump['destination_word_address']),
            'sine_lookup': sine}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directories', nargs='+', type=Path)
    args = parser.parse_args()
    for directory in args.directories:
        print(json.dumps(map_image(directory)))
