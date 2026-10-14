#!/usr/bin/env python3
"""Exercise the loaded VST3's actual getState/setState callbacks via its private API.

Uses RAM sound changes only. Run with Live's test instance idle.
First phase creates edited.vtrstate; --restart verifies it in a fresh instance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import time
import zlib
from ti_control import discover, request


def decode(path):
    data = Path(path).read_bytes()
    magic, version, count = struct.unpack_from('<III', data)
    assert (magic, version, count) == (0x56545257, 2, 2672)
    offset = 12 + count * 12
    flags, size = struct.unpack_from('<II', data, offset)
    patch = data[offset + 8:offset + 8 + size]
    assert len(data) == offset + 12 + size
    assert zlib.crc32(patch) == struct.unpack_from('<I', data, offset + 8 + size)[0]
    return data, patch, flags


def sound(patch):
    # TI normalizes the edit flag and original-bank/program metadata on recall.
    return bytes(v for i, v in enumerate(patch) if i not in (5, 7, 8, 10, 11, 12, 265, 522))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--socket')
    parser.add_argument('--output', type=Path, default=Path('analysis/daw-validation/project-state'))
    parser.add_argument('--restart', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    instances = [s for s in discover() if s['host_processing']]
    endpoint = args.socket or (instances[0]['socket'] if len(instances) == 1 else None)
    assert endpoint, 'Select one active instance'
    report = {'socket': endpoint, 'steps': []}

    def run(command):
        start = time.monotonic()
        state = request(endpoint, command)
        assert state['ok'], state
        command_id = state.get('request_id', 0)
        while state['completed_request_id'] < command_id or state['busy'] or state.get('state_recall_pending'):
            assert time.monotonic() - start < 20, state
            time.sleep(.1); state = request(endpoint, 'status')
        assert state['last_command_succeeded'], state
        report['steps'].append({'command': command, 'elapsed': time.monotonic() - start, 'state': state})
        print(command, state['status'], flush=True)
        return state

    def save(name):
        path = out / (name + '.vtrstate')
        run({'command': 'state-save', 'path': str(path)})
        raw, patch, flags = decode(path)
        (out / (name + '.syx')).write_bytes(patch)
        return patch

    def recall(name):
        return run({'command': 'state-load', 'path': str(out / (name + '.vtrstate'))})

    baseline = request(endpoint, 'status'); report['baseline'] = baseline
    if args.restart:
        assert not baseline['connected'] and baseline['lifetime_counters']['capture_callbacks'] == 0
        saved_report = json.loads((out / 'roundtrip.json').read_text())
        assert baseline['pid'] != saved_report['baseline']['pid'], 'Must use a fresh Live process'
        state = recall('edited')  # No explicit Connect: the persisted connection intent must do this.
        assert state['connected']
        _, expected, _ = decode(out / 'edited.vtrstate')
        actual = save('after-restart')
        assert sound(actual) == sound(expected)
        report['exact_sound_after_restart'] = True
        report['full_patch_byte_differences'] = [i for i, (a, b) in enumerate(zip(expected, actual)) if a != b]
    else:
        assert baseline['connected'] and baseline['state_format'] == 2
        save('initial')
        run('test-tone'); before = save('before-edit')
        run({'command': 'cutoff', 'value': 37}); edited = save('edited')
        assert sound(before) != sound(edited), 'Hardware edit must be captured, not cached source data'
        report['after_load_edit_offsets'] = [i for i, (a, b) in enumerate(zip(before, edited)) if a != b]
        assert edited[49] == 37, 'TI CC40 cutoff must be present in saved RAM patch'
        run({'command': 'load', 'bank': 2, 'preset': 0}); different = save('different')
        assert sound(edited) != sound(different)
        recall('edited'); restored = save('restored')
        assert sound(restored) == sound(edited)
        report['exact_sound_roundtrip'] = True
        report['full_patch_byte_differences'] = [i for i, (a, b) in enumerate(zip(edited, restored)) if a != b]
        # Malformed states must not alter the running sound or disconnect hardware.
        raw = (out / 'edited.vtrstate').read_bytes()
        invalid = {'truncated': raw[:-1], 'bad-crc': raw[:-1] + bytes([raw[-1] ^ 1]),
                   'unknown-version': raw[:4] + struct.pack('<I', 99) + raw[8:]}
        for name, data in invalid.items():
            path = out / (name + '.vtrstate'); path.write_bytes(data)
            result = request(endpoint, {'command': 'state-load', 'path': str(path)})
            assert not result['ok'] and result['connected'], result
            report['steps'].append({'rejected_state': name, 'state': result})
        assert sound(save('after-invalid')) == sound(edited)
        # A disconnected snapshot remains the plug-in sound, not restored external Single.
        run('disconnect'); offline = save('offline')
        assert sound(offline) == sound(edited)
        assert decode(out / 'offline.vtrstate')[2] == 0
        recall('edited'); assert sound(save('after-reconnect')) == sound(edited)
        # Repeat across several real hardware edits to catch stale cached snapshots.
        for value in (23, 61, 94):
            run({'command': 'cutoff', 'value': value}); changed = save('edit-' + str(value))
            assert changed[49] == value
            recall('edited'); assert sound(save('repeat-' + str(value))) == sound(edited)
    report['final'] = request(endpoint, 'status')
    counters = report['final']['lifetime_counters']
    for key in ('capture_errors', 'fifo_overruns', 'fifo_underruns'):
        assert counters[key] == baseline['lifetime_counters'][key], (key, counters)
    report['success'] = True
    (out / ('restart.json' if args.restart else 'roundtrip.json')).write_text(json.dumps(report, indent=2))
    print(json.dumps({k: v for k, v in report.items() if k not in ('steps', 'baseline', 'final')}, indent=2))


if __name__ == '__main__':
    main()
