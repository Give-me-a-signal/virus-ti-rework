#!/usr/bin/env python3
"""Build an experimental offline, same-footprint TI 5.1.7 wavetable replacement.

Edits packed spectral bytes only, preserves descriptor fields, updates the affected
host bank CRCs, and verifies all other image bytes are unchanged. Never flashes.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'analysis/firmware-images'
EXPECTED = '78844fced13ca92d34f6a62806321c0826a742666fa3aa90a6ed40d538ca9d87'


def check(value, message):
    if not value:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def build(source, table_id, coefficients, output):
    check(1 <= table_id <= 99, 'This tool handles stored IDs 1..99; ID 0 has no descriptor')
    original = source.read_bytes()
    check(sha(original) == EXPECTED, 'Unsupported or modified source firmware')
    check(output.resolve() != source.resolve(), 'Refusing to overwrite original')
    directory = json.loads((ROOT / 'analysis/wavetable-trace/structures/wavetable-directory.json').read_text())
    entry = next(e for e in directory['entries'] if e['id'] == table_id)
    rows = list(csv.reader(coefficients.open()))
    bins, frames = entry['bins_per_frame'], entry['frame_count']
    check(rows[0] == ['frame'] + [f'bin_{i}' for i in range(bins)], 'CSV header does not match table shape')
    check(len(rows) == frames + 1, 'Changing frame count is unsupported')
    payload = bytearray()
    for frame, row in enumerate(rows[1:]):
        check(len(row) == bins + 1 and int(row[0]) == frame, 'Invalid frame index or bin count')
        values = [int(v) for v in row[1:]]
        check(all(0 <= v <= 127 for v in values), 'Coefficients must be integers 0..127')
        payload.extend(values)
    manifest = json.loads((BASE / 'decoded/vti/manifest.json').read_text())
    loadmap = json.loads((BASE / 'decoded/vti/dsp-load/map.json').read_text())
    block = next(b for b in loadmap['blocks'] if b['destination_word_address'] == 0x4f000)
    chunks = {c['tag']: c for c in manifest['chunks']}
    segment_ranges = []
    start = 0
    for segment in manifest['primary_dsp_word_blocks']:
        segment_ranges.append((start, start + segment['word_count'], segment))
        start += segment['word_count']

    def locate(payload_byte):
        word = block['stream_data_word_offset'] + entry['descriptor_word_address'] + 4 - 0x4f000 + payload_byte // 3
        word += manifest['dsp_boot']['word_count'] + 2
        for start, end, segment in segment_ranges:
            if start <= word < end:
                return segment['tag'], 3 + (word - start) * 3 + payload_byte % 3
        raise ValueError('Source byte outside DSP banks')

    tables = bytes.fromhex(json.loads((BASE / 'updater-crc-tables.json').read_text())['result']['bytes_hex'])
    check(len(tables) == 512, 'Invalid CRC tables')

    def crc(data):
        value = 0xffff
        for byte in data:
            index = (value >> 8) ^ byte
            value = (((value & 255) ^ tables[index]) << 8) | tables[index + 256]
        return value

    def file_offset(tag, bank_offset):
        chunk = chunks[tag]
        check(chunk['encoded_size'] == 0x8c02, 'Unsupported bank encoding')
        return chunk['source_payload_offset'] + (bank_offset // 32) * 35 + 3 + bank_offset % 32

    modified = bytearray(original)
    bank_data = {}
    allowed = set()
    original_payload = bytearray()
    for i, value in enumerate(payload):
        tag, offset = locate(i)
        encoded = file_offset(tag, offset)
        original_payload.append(original[encoded])
        if tag not in bank_data:
            chunk = chunks[tag]
            start = chunk['source_payload_offset']
            raw = bytearray(b''.join(original[start + 35*j + 3:start + 35*j + 35] for j in range(1024)))
            trailer = original[start + 35840:start + 35842]
            check(crc(raw) == int.from_bytes(trailer, 'big'), 'Original affected-bank CRC mismatch')
            bank_data[tag] = raw
        modified[encoded] = value
        bank_data[tag][offset] = value
        allowed.add(encoded)
    check(sha(original_payload) == entry['spectral_payload_sha256'], 'Table source mapping mismatch')
    crc_records = []
    for tag, raw in bank_data.items():
        offset = chunks[tag]['source_payload_offset'] + 35840
        new_crc = crc(raw)
        modified[offset:offset+2] = new_crc.to_bytes(2, 'big')
        allowed.update((offset, offset+1))
        crc_records.append({'tag': tag, 'old_crc': original[offset:offset+2].hex(), 'new_crc': f'{new_crc:04x}'})
        decoded = b''.join(modified[chunks[tag]['source_payload_offset']+35*j+3:chunks[tag]['source_payload_offset']+35*j+35] for j in range(1024))
        check(decoded == raw and crc(decoded) == new_crc, 'Repacked bank verification failed')
    changes = [i for i, (a, b) in enumerate(zip(original, modified)) if a != b]
    check(len(modified) == len(original) and all(i in allowed for i in changes), 'Unexpected image change')
    recovered = bytes(modified[file_offset(*locate(i))] for i in range(len(payload)))
    check(recovered == payload, 'Replacement re-extraction mismatch')
    check(sha(source.read_bytes()) == EXPECTED, 'Original changed during operation')
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        check(output.read_bytes() == modified, 'Refusing to overwrite a different existing output')
    else:
        output.write_bytes(modified)
    result = {'status': 'experimental offline image; no hardware acceptance or flashing tested',
              'source': str(source.resolve()), 'source_sha256': sha(original),
              'output': str(output.resolve()), 'output_sha256': sha(modified),
              'table_id': table_id, 'name': entry['name'], 'frames': frames, 'bins': bins,
              'spectral_payload_sha256': sha(payload), 'changed_bytes': len(changes),
              'changed_file_offsets': changes, 'bank_crcs': crc_records,
              'checks': {'original_hash': True, 'descriptor_and_directory_preserved': True,
                         'only_spectral_bytes_and_affected_bank_crcs_changed': True,
                         're_extracted_coefficients_equal_csv': True, 'size_unchanged': True}}
    output.with_suffix(output.suffix + '.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=BASE / 'vti.bin')
    parser.add_argument('--id', type=int, required=True)
    parser.add_argument('--coefficients', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = build(args.source, args.id, args.coefficients, args.output)
    print(json.dumps({k:v for k,v in result.items() if k != 'changed_file_offsets'}, indent=2))
