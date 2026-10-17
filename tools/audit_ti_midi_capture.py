#!/usr/bin/env python3
"""Audit saved private USB MIDI traffic without opening hardware."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path


def audit(path, allow_timing=False):
    raw = path.read_bytes()
    assert len(raw) % 4 == 0
    messages, pending = [], bytearray()
    stamps, stamp_pending, tagged = 0, False, []
    lengths = {4: 3, 5: 1, 6: 2, 7: 3, 8: 3, 9: 3, 11: 3}
    for at in range(0, len(raw), 4):
        cable, cin = raw[at] >> 4, raw[at] & 15
        if allow_timing and cable == 5:
            assert not pending and not stamp_pending
            assert raw[at:at+2] == bytes((0x53, 0xf5)) and raw[at+2] < 128 and raw[at+3] < 128
            stamps += 1; stamp_pending = True
            continue
        if allow_timing and cin in (3, 15):
            assert cable == 2 and stamp_pending and not pending
            data = raw[at+1:at+4] if cin == 3 else raw[at+1:at+2]
            assert cin == 3 and data[0] == 0xf2 and all(b < 128 for b in data[1:]) or cin == 15 and data[0] in (0xfe, 0xf8) and raw[at+2:at+4] == bytes(2)
            messages.append(data); tagged.append(True); stamp_pending = False
            continue
        assert cable == 2 and cin in lengths, (at, raw[at:at + 4].hex())
        data = raw[at + 1:at + 1 + lengths[cin]]
        if cin in (4, 5, 6, 7):
            if not pending:
                assert data[0] == 0xf0
            pending.extend(data)
            if cin != 4:
                assert pending[-1] == 0xf7
                messages.append(bytes(pending)); tagged.append(stamp_pending); stamp_pending = False; pending.clear()
        else:
            assert not pending
            messages.append(data); tagged.append(stamp_pending); stamp_pending = False
    assert not pending and not stamp_pending
    kinds = Counter()
    for data, stamped in zip(messages, tagged):
        if allow_timing and stamped and data[0] in (0xf2, 0xfe, 0xf8):
            kinds[f'Clock {data[0]:02x}'] += 1
            continue
        if data[0] != 0xf0:
            assert len(data) == 3 and data[0] & 0xf0 in (0x80, 0x90, 0xb0)
            assert all(b < 128 for b in data[1:])
            kinds[f'MIDI {data[0] & 0xf0:02x}'] += 1
            continue
        assert data[:5] == bytes.fromhex('f0 00 20 33 01') and data[-1] == 0xf7
        assert all(b < 128 for b in data[1:-1])
        cmd = data[6]
        allowed = False
        if cmd in (0x30, 0x31):
            allowed = len(data) == 10 and data[7] == 0
        elif cmd == 0x35:
            allowed = len(data) in (8, 10)
        elif cmd == 0x10:
            allowed = len(data) in (267, 524) and data[7] == 0 and (data[8] < 16 or data[8] == 64)
        elif cmd == 0x11:
            allowed = len(data) == 267 and data[7:9] == bytes((0, 127))
        elif cmd in (0x70, 0x71, 0x6e, 0x6f):
            allowed = len(data) == 11
        elif cmd == 0x72:
            allowed = len(data) == 11 and data[7] < 16 and (data[8] in (32, 33) or data[8] == 41 and data[9] <= 17)
        elif cmd == 0x73:
            allowed = len(data) == 11 and data[7] == 0 and (data[8] == 122 and data[9] in (0, 3) or data[8] in (58, 59, 60) and data[9] == 1 or data[8] == 9 and data[9] in (0, 1))
        if allow_timing and stamped and data == bytes.fromhex('f0 00 20 33 01 10 73 00 22 00 f7'):
            allowed = True
        assert allowed, ('Unexpected command', data.hex())
        kinds[f'SysEx {cmd:02x}'] += 1
    return {'path': str(path.resolve()), 'bytes': len(raw), 'messages': len(messages),
            'sha256': hashlib.sha256(raw).hexdigest(), 'private_timestamp_words': stamps, 'command_counts': dict(kinds),
            'only_known_read_note_patch_or_usb_mode_commands': True}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--allow-timing', action='store_true', help='Also permit exact stamped clock/ping messages; never firmware or bank writes')
    parser.add_argument('paths', type=Path, nargs='+')
    args = parser.parse_args()
    print(json.dumps([audit(path, args.allow_timing) for path in args.paths], indent=2))
