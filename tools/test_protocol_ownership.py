#!/usr/bin/env python3
"""Real AU/VST3 services must refuse a held lease before any hardware I/O.

The lease holder is a test process, not a connected synth. This checks refusal
and cleanup, not uninterrupted physical playback or successful device ownership.
Never removes the shared lock file; it may be used by another application later.
"""
import argparse
import fcntl
import os
from pathlib import Path
import subprocess
import tempfile
import time
from ti_control import discover, request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('host', type=Path)
parser.add_argument('vst3', type=Path)
parser.add_argument('au', type=Path)
args = parser.parse_args()
base = Path.home() / 'Library/Application Support/VirusTIRework'
base.mkdir(parents=True, exist_ok=True)
lease = os.open(base / 'hardware.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
try:
    try:
        fcntl.flock(lease, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise SystemExit('Existing hardware owner: not running the contention test')
    with tempfile.TemporaryDirectory() as root:
        root = Path(root); children = []
        try:
            for kind, bundle in [('vst3', args.vst3), ('au', args.au)]:
                log = (root / (kind + '.log')).open('w')
                child = subprocess.Popen([str(args.host), kind, str(bundle), str(root / (kind + '.f32')),
                                          str(root / 'never-trigger'), '48000', '256', '30'], stdout=log, stderr=log)
                children.append((child, log))
                deadline = time.monotonic() + 15; endpoint = None
                while time.monotonic() < deadline:
                    matches = [s for s in discover() if s['pid'] == child.pid]
                    if matches:
                        endpoint = matches[0]['socket']; break
                    assert child.poll() is None, (root / (kind + '.log')).read_text()
                    time.sleep(.1)
                assert endpoint
                for attempt in range(3):
                    result = request(endpoint, 'connect')
                    assert result['ok'], result
                    target = result['request_id']; deadline = time.monotonic() + 5
                    while result['busy'] or result['completed_request_id'] < target:
                        assert time.monotonic() < deadline
                        time.sleep(.05); result = request(endpoint, 'status')
                    assert not result['last_command_succeeded'] and 'owned by another' in result['status']
                    assert not result['connected'] and not result['device_lease_held']
                    assert not result['snapshot_directory']
                    assert result['lifetime_counters']['capture_callbacks'] == 0
                    assert result['lifetime_counters']['midi_sent'] == 0
                print(kind, 'refused 3 connections before capture/MIDI/snapshot setup')
            # A loser's cleanup must not unlock or unlink the real owner's file.
            other = os.open(base / 'hardware.lock', os.O_RDWR | os.O_NOFOLLOW)
            try:
                try:
                    fcntl.flock(other, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError:
                    pass
                else:
                    raise AssertionError('Contender released the owner lease')
            finally:
                os.close(other)
        finally:
            for child, log in children:
                if child.poll() is None:
                    child.terminate()
                assert child.wait(timeout=15) == 0
                log.close()
finally:
    os.close(lease)
print('Cross-format process contention and repeated refused-connection cleanup passed (no hardware I/O)')
