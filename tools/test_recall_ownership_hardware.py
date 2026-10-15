#!/usr/bin/env python3
"""Bounded TI Desktop RAM recall test using actual AU/VST3 bundle hosts.

No factory fixture, firmware transfer, stored-bank request/write, USB reset or
DAW termination. Requires the normal Rework MIDI driver and explicit hardware
connection. Raw snapshots and recordings must stay in a fresh ignored folder.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
from ti_control import discover, request
from test_multipart_api import decode, sound
from audit_ti_midi_capture import audit

def wait(endpoint, command):
    result = request(endpoint, command); assert result['ok'], result
    target = result.get('request_id', 0); deadline = time.monotonic() + 150
    while result['busy'] or result['state_recall_pending'] or result['completed_request_id'] < target:
        assert time.monotonic() < deadline, result
        time.sleep(.1); result = request(endpoint, 'status')
    assert result['last_command_succeeded'], result
    return result

def verify_restore(session, helper):
    folder = session / 'restoration-helper' if helper else session
    for p in range(16):
        assert sound((session / f'before-part{p}.syx').read_bytes()) == sound((folder / f'restored-part{p}.syx').read_bytes()), p
    assert (session / 'before-single.syx').read_bytes() == (folder / 'restored-single.syx').read_bytes()
    a = (session / 'before-multi.syx').read_bytes(); b = (folder / 'restored-multi.syx').read_bytes()
    assert len(a) == len(b) == 267 and all(x == y for i, (x, y) in enumerate(zip(a, b)) if i not in (10, 265))
    return [audit(p, True) for p in folder.glob('private-out*.bin')]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', type=Path, required=True)
    parser.add_argument('--vst3', type=Path, required=True)
    parser.add_argument('--au', type=Path, required=True)
    parser.add_argument('--format', choices=['vst3', 'au'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timing', action='store_true', help='Explicit opt-in to experimental marker transport')
    args = parser.parse_args(); out = args.output.resolve(); out.mkdir(parents=True, exist_ok=False)
    report = {'format': args.format, 'rate': 48000, 'block': 256, 'timing_requested': args.timing, 'passed': False}
    def persist(): (out / 'result.json').write_text(json.dumps(report, indent=2))
    children = []; session = None
    def launch(kind):
        log = (out / (kind + '.log')).open('w'); env = os.environ.copy()
        env['VTR_ENABLE_MARKER_TIMING'] = '1' if args.timing else '0'
        child = subprocess.Popen([str(args.host.resolve()), kind, str(getattr(args, kind).resolve()),
                                  str(out / (kind + '.f32')), str(out / 'never-trigger'), '48000', '256', '1800'], stdout=log, stderr=log, env=env)
        children.append((child, log)); deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            matches = [s for s in discover() if s['pid'] == child.pid]
            if matches: return child, matches[0]['socket']
            assert child.poll() is None, 'Host failed to load'; time.sleep(.1)
        raise TimeoutError('No bundle API endpoint')
    try:
        owner, endpoint = launch(args.format)
        status = wait(endpoint, 'connect'); assert status['connected'] and status['device_lease_held']
        session = Path(status['snapshot_directory']); report['session'] = str(session); report['connected'] = status; persist()
        assert len(list(session.glob('before-*.syx'))) == 18
        if args.timing: assert status['timing_status'] == 'locked', 'Marker calibration did not lock; stop without further writes'
        baseline = out / 'baseline.vtrstate'; wait(endpoint, {'command': 'state-save', 'path': str(baseline)})
        expected, multi, _ = decode(baseline)
        competitor, other = launch('au' if args.format == 'vst3' else 'vst3')
        denied = request(other, 'connect'); target = denied['request_id']; deadline = time.monotonic() + 5
        while denied['busy'] or denied['completed_request_id'] < target:
            assert time.monotonic() < deadline; time.sleep(.1); denied = request(other, 'status')
        assert not denied['last_command_succeeded'] and not denied['connected'] and not denied['device_lease_held']
        assert not denied['snapshot_directory'] and denied['lifetime_counters']['capture_callbacks'] == 0
        assert denied['lifetime_counters']['midi_sent'] == 0
        competitor.terminate(); assert competitor.wait(timeout=15) == 0
        report['competing_format_refused_before_io'] = True; report['recalls'] = []; persist()
        for cycle in range(10):
            value = 37 if expected[0][49] != 37 else 71
            wait(endpoint, {'command': 'cutoff', 'part': 1, 'value': value}); time.sleep(.15)
            changed = out / f'changed-{cycle}.vtrstate'; wait(endpoint, {'command': 'state-save', 'path': str(changed)})
            assert decode(changed)[0][0][49] == value, 'Supported cutoff edit did not reach RAM'
            wait(endpoint, {'command': 'state-load', 'path': str(baseline)})
            actual = out / f'recalled-{cycle}.vtrstate'; wait(endpoint, {'command': 'state-save', 'path': str(actual)})
            parts, routing, _ = decode(actual)
            assert list(map(sound, parts)) == list(map(sound, expected))
            assert all(a == b for i, (a, b) in enumerate(zip(multi, routing)) if i not in (10, 265))
            report['recalls'].append({'cycle': cycle + 1, 'sixteen_sounds_and_routing_match': True}); persist()
        wait(endpoint, 'disconnect'); report['manual_restore'] = verify_restore(session, False); persist()
        # A new connection preserves a fresh baseline for normal-exit recovery.
        status = wait(endpoint, 'connect'); session = Path(status['snapshot_directory']); report['exit_session'] = str(session); persist()
        owner.terminate(); assert owner.wait(timeout=15) == 0
        deadline = time.monotonic() + 150
        while time.monotonic() < deadline:
            state = json.loads((session / 'restoration-status.json').read_text())
            if state['state'] in ('restored', 'failed'): break
            time.sleep(.2)
        assert state['state'] == 'restored', state
        report['normal_exit_restore'] = verify_restore(session, True); report['passed'] = True; persist()
    except BaseException as error:
        report['error'] = repr(error); persist(); raise
    finally:
        for child, log in children:
            if child.poll() is None: child.terminate()
            child.wait(timeout=20); log.close()
        if session:
            # Even a failed test leaves the device powered and recovery intact.
            deadline = time.monotonic() + 150
            while time.monotonic() < deadline:
                status = json.loads((session / 'restoration-status.json').read_text())
                if status['state'] in ('restored', 'completed-by-plugin', 'failed'): break
                time.sleep(.2)
            report['final_recovery_status'] = status; persist()
    print('PASS: ten RAM recalls, cross-format refusal, manual and normal-exit recovery')

if __name__ == '__main__': main()
