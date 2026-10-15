#!/usr/bin/env python3
"""One bounded timing measurement using an owner-captured RAM patch.

Stops on failed lock/readback/measurement; recovery is always allowed to finish.
No bank synchronization, firmware updates, stored-bank writes or USB resets.
"""
import argparse
import json
import math
import os
from pathlib import Path
import re
import subprocess
import time
from ti_control import discover, request
from test_recall_ownership_hardware import wait, verify_restore
from analyze_marker_timing import analyze

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host', type=Path, required=True)
    p.add_argument('--bundle', type=Path, required=True)
    p.add_argument('--format', choices=['vst3', 'au'], required=True)
    p.add_argument('--patch', type=Path, required=True, help='Private patch captured from this hardware; no factory fixture')
    p.add_argument('--rate', type=int, choices=[44100, 48000, 96000], required=True)
    p.add_argument('--block', type=int, choices=[64, 256, 512], required=True)
    p.add_argument('--bus', type=int, choices=[1, 2, 3], required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args(); out = args.output.resolve(); out.mkdir(parents=True, exist_ok=False)
    patch = args.patch.read_bytes()
    assert len(patch) == 524 and patch[7] == 0 and patch[8] < 16, 'Require a captured part edit buffer'
    report = {'passed': False, 'format': args.format, 'rate': args.rate, 'block': args.block, 'bus': args.bus}
    def persist(): (out / 'result.json').write_text(json.dumps(report, indent=2))
    logpath = out / 'host.log'; recording = out / 'audio.f32'; trigger = out / 'start-sequence'
    env = os.environ.copy(); env['VTR_ENABLE_MARKER_TIMING'] = '1'
    log = logpath.open('w'); child = subprocess.Popen([str(args.host.resolve()), args.format, str(args.bundle.resolve()),
        str(recording), str(trigger), str(args.rate), str(args.block), '300'], stdout=log, stderr=log, env=env)
    session = None
    try:
        deadline = time.monotonic() + 15; endpoint = None
        while time.monotonic() < deadline:
            matches = [s for s in discover() if s['pid'] == child.pid]
            if matches: endpoint = matches[0]['socket']; break
            assert child.poll() is None, logpath.read_text(); time.sleep(.1)
        assert endpoint
        state = wait(endpoint, 'connect'); session = Path(state['snapshot_directory']); report['session'] = str(session); report['connected'] = state; persist()
        assert state['timing_status'] == 'locked', 'No verified marker pattern; no test notes sent'
        wait(endpoint, {'command': 'load-file', 'path': str(args.patch.resolve()), 'preset': 0, 'part': 1})
        wait(endpoint, {'command': 'route', 'part': 1, 'bus': args.bus})
        # Patch upload is a potentially long SysEx operation. Do not send any
        # performance events until it has finished and the stream is stable.
        time.sleep(1); before = request(endpoint, 'status'); report['before'] = before; persist()
        trigger.touch(); time.sleep(35)
        after = request(endpoint, 'status'); report['after'] = after; persist()
        for counter in ('capture_errors', 'fifo_underruns', 'fifo_overruns'):
            assert after['lifetime_counters'][counter] == before['lifetime_counters'][counter], counter
        assert after['timing_late_events'] == before['timing_late_events'], 'Late stamped events'
        assert after['timing_overflow'] == before['timing_overflow'], 'Performance overflow'
        child.terminate(); assert child.wait(timeout=15) == 0
        text = logpath.read_text(); assert 'note_on=256 note_off=256' in text, text
        first = int(re.search(r'FIXTURE_START (\d+)', text)[1])
        latency = before.get('reported_latency_samples', math.ceil((2048 + max(2 * math.ceil(args.block * 48000 / args.rate), 1024) + 32) * args.rate / 48000))
        report['measurement'] = analyze(recording, args.rate, first, latency, args.bus)
        report['passed'] = report['measurement']['passed']; persist()
        assert report['passed'], report['measurement']
    except BaseException as error:
        report['error'] = repr(error); persist(); raise
    finally:
        if child.poll() is None: child.terminate()
        child.wait(timeout=20); log.close()
        if session:
            deadline = time.monotonic() + 150
            while time.monotonic() < deadline:
                status = json.loads((session / 'restoration-status.json').read_text())
                if status['state'] in ('restored', 'failed'): break
                time.sleep(.2)
            report['restoration_status'] = status
            if status['state'] == 'restored': report['restoration_audit'] = verify_restore(session, True)
            else: report['passed'] = False
            persist(); assert status['state'] == 'restored', status
    print(json.dumps(report['measurement'], indent=2))

if __name__ == '__main__': main()
