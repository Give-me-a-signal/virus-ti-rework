#!/usr/bin/env python3
"""Offline negative tests for the private timestamp capture auditor."""
from pathlib import Path
from tempfile import TemporaryDirectory
from audit_ti_midi_capture import audit

with TemporaryDirectory() as root:
    path = Path(root) / 'traffic.bin'
    stamp = bytes.fromhex('53 f5 7f 7f')
    path.write_bytes(stamp + bytes.fromhex('29 90 3c 20 53 f5 00 00 2f f8 00 00'))
    assert audit(path, True)['private_timestamp_words'] == 2
    for bad in [stamp, stamp + stamp, bytes.fromhex('2f f8 00 00'),
                stamp + bytes.fromhex('2f ff 00 00'), bytes.fromhex('53 f5 80 00')]:
        path.write_bytes(bad)
        try:
            audit(path, True)
        except AssertionError:
            pass
        else:
            raise AssertionError('Accepted malformed or unapproved private traffic')
print('Timing audit: stamped messages accepted; malformed/unapproved traffic rejected')
