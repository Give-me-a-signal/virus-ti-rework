#!/usr/bin/env python3
"""Extract TI 5.1.7 spectral-table structures. Offline, original files read-only.

The directory was independently found by pointer/record-size analysis; the ID
dispatch is corroborated by DSP disassembly at P:05fc06 through P:05fc38.
Byte values are retained exactly; this tool does not assume an amplitude law.
"""
import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'analysis/firmware-images/decoded/vti'
OUT = ROOT / 'analysis/wavetable-trace/structures'
BASE = 0x4F000
DIRECTORY = 0x5FF2B


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((SOURCE / 'manifest.json').read_text())
    chunks = {e['tag']: e for e in manifest['chunks']}
    loadmap = json.loads((SOURCE / 'dsp-load/map.json').read_text())
    block = next(b for b in loadmap['blocks'] if b['destination_word_address'] == BASE)
    path = SOURCE / 'dsp-load' / block['file']
    data = path.read_bytes()
    words = [int.from_bytes(data[i:i+3], 'big') for i in range(0, len(data), 3)]
    names = json.loads((SOURCE / 'wavetable-name-offsets.json').read_text())['entries']
    original_image = (SOURCE.parent.parent / 'vti.bin').read_bytes()

    def source_byte(address, byte_in_word=0):
        stream_word = block['stream_data_word_offset'] + address - BASE
        remaining = stream_word + manifest['dsp_boot']['word_count'] + 2
        for segment in manifest['primary_dsp_word_blocks']:
            if remaining < segment['word_count']:
                bank_offset = 3 + remaining * 3 + byte_in_word
                entry = chunks[segment['tag']]
                assert bank_offset < 32768
                encoded_offset = bank_offset if entry['encoded_size'] == 0x8000 else (bank_offset // 32) * 35 + 3 + bank_offset % 32
                return {'tag': segment['tag'], 'decoded_bank_offset': bank_offset,
                        'original_image_offset': entry['source_payload_offset'] + encoded_offset}
            remaining -= segment['word_count']
        raise ValueError('Address outside loader stream')

    def word(address):
        return words[address - BASE]

    entries = []
    assert word(DIRECTORY) == word(DIRECTORY + 1) == 0
    for index in range(1, 100):
        address = word(DIRECTORY + index * 2)
        assert word(DIRECTORY + index * 2 + 1) == 0
        header = words[address-BASE:address-BASE+4]
        log2_bins, frames, flags, scale = header
        assert log2_bins in (5, 6, 7, 8) and flags in (0, 1)
        bins = 1 << log2_bins
        byte_count = bins * frames
        stored_words = (byte_count + 2) // 3
        end = address + 4 + stored_words
        if index < 99:
            assert end == word(DIRECTORY + (index+1) * 2), (index, hex(end))
        offset = (address-BASE) * 3
        record = data[offset:offset+(4+stored_words)*3]
        payload = record[12:12+byte_count]
        padding = record[12+byte_count:]
        assert len(payload) == byte_count and max(payload) == 127
        assert all(v == 0 for v in padding)
        entry = {'id': index, 'name': names[index]['name'],
                 'directory_word_address': DIRECTORY + index*2,
                 'descriptor_word_address': address, 'end_word_address_exclusive': end,
                 'header_words': header, 'bins_per_frame': bins, 'frame_count': frames,
                 'flag_word': flags, 'scale_word': scale,
                 'spectral_byte_count': byte_count, 'record_word_count': 4+stored_words,
                 'padding_hex': padding.hex(),
                 'header_source': source_byte(address), 'payload_source': source_byte(address+4),
                 'last_record_byte_source': source_byte(end-1, 2),
                 'record_sha256': hashlib.sha256(record).hexdigest(),
                 'spectral_payload_sha256': hashlib.sha256(payload).hexdigest()}
        entries.append(entry)
        if index == 1:
            (OUT / 'id001-HarmncSweep.record24be.bin').write_bytes(record)
            (OUT / 'id001-HarmncSweep.spectral-u8.bin').write_bytes(payload)
            with (OUT / 'id001-HarmncSweep.spectral-u8.csv').open('w') as f:
                writer = csv.writer(f)
                writer.writerow(['frame'] + [f'bin_{i}' for i in range(bins)])
                writer.writerows([frame]+list(payload[frame*bins:(frame+1)*bins]) for frame in range(frames))
            # Packed container record headers mean file offsets are discontinuous.
            byte_map = []
            for i, value in enumerate(payload):
                location = source_byte(address+4+i//3, i%3)
                assert original_image[location['original_image_offset']] == value
                byte_map.append({'payload_index': i, 'frame': i//bins, 'bin': i%bins,
                                 'value': value, **location})
            (OUT / 'id001-HarmncSweep.source-byte-map.json').write_text(json.dumps(byte_map, indent=2)+'\n')
            # Exact repacking test, independent of synthesis/amplitude semantics.
            rebuilt = b''.join(v.to_bytes(3, 'big') for v in header) + bytes(payload) + padding
            assert rebuilt == record
    result = {'source': str(path), 'source_sha256': hashlib.sha256(data).hexdigest(),
              'directory_word_address': DIRECTORY, 'directory_entries': 100,
              'id0': {'pointer': 0, 'second_word': 0, 'name': names[0]['name']},
              'validation': {'adjacent_record_sizes_match': 98, 'tables_parsed': 99,
                             'all_payload_bytes_0_to_127': True, 'all_padding_zero': True,
                             'id1_every_byte_matches_original_encoded_image': True,
                             'id1_record_repack_byte_identical': True},
              'interpretation': 'Spectral-bin bytes, not PCM; amplitude transfer, phase flag, scale and FFT rendering require code tracing.',
              'entries': entries}
    (OUT / 'wavetable-directory.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({'output': str(OUT), 'validation': result['validation'],
                      'id1': entries[0], 'id99_end': hex(entries[-1]['end_word_address_exclusive'])}, indent=2))


if __name__ == '__main__':
    main()
