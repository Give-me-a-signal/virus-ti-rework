#!/usr/bin/env python3
"""Load actual AU/VST3 bundles without Connect; assert zero hardware activity."""
import argparse
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
with tempfile.TemporaryDirectory() as root:
    root = Path(root)
    for kind, bundle in [('vst3', args.vst3), ('au', args.au)]:
        audio = root / (kind + '.f32')
        env = os.environ.copy(); env.pop('VTR_ENABLE_MARKER_TIMING', None)
        with (root / (kind + '.log')).open('w') as log:
            child = subprocess.Popen([str(args.host), kind, str(bundle), str(audio),
                                      str(root / 'never-trigger'), '48000', '256', '30'],
                                     stdout=log, stderr=log, env=env)
            try:
                deadline = time.monotonic() + 15
                endpoint = None
                while time.monotonic() < deadline:
                    matches = [s for s in discover() if s['pid'] == child.pid]
                    if matches:
                        endpoint = matches[0]['socket']; break
                    assert child.poll() is None, (root / (kind + '.log')).read_text()
                    time.sleep(.1)
                assert endpoint, 'No bundle API endpoint'
                time.sleep(.3)
                status = request(endpoint, 'status')
                assert not status['connected'] and not status['device_lease_held']
                assert not status['snapshot_directory']
                assert status['lifetime_counters']['capture_callbacks'] == 0
                assert status['lifetime_counters']['midi_sent'] == 0
                child.terminate(); assert child.wait(timeout=15) == 0
                assert audio.stat().st_size > 0
                assert not any(audio.read_bytes()), 'Disconnected output must be zero'
            finally:
                if child.poll() is None:
                    child.terminate(); child.wait(timeout=15)
        print(kind, 'actual bundle discovery/editor-free real-time render: no hardware activity')
