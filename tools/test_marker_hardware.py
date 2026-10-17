#!/usr/bin/env python3
"""One bounded timing measurement using an owner-captured RAM patch.

Stops on failed lock/readback/measurement; recovery is always allowed to finish.
No bank synchronization, firmware updates, stored-bank writes or USB resets.
"""
import argparse
import fcntl
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
from audit_ti_midi_capture import audit

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
    p.add_argument('--competitor-bundle', type=Path, help='Opposite format; verify refusal during sounding playback')
    p.add_argument('--immediate', action='store_true', help='Verify fallback playback only; do not apply synchronized timing acceptance')
    p.add_argument('--check-handoff', action='store_true', help='Also refuse the competing format while the exit helper restores')
    args = p.parse_args()
    if args.check_handoff and not args.competitor_bundle: p.error('--check-handoff requires --competitor-bundle')
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=False)
    patch = args.patch.read_bytes()
    assert len(patch) == 524 and patch[7] == 0 and patch[8] < 16, 'Require a captured part edit buffer'
    report = {'passed': False, 'format': args.format, 'rate': args.rate, 'block': args.block, 'bus': args.bus,
              'timing_requested': not args.immediate}
    def persist(): (out / 'result.json').write_text(json.dumps(report, indent=2))
    logpath = out / 'host.log'; recording = out / 'audio.f32'; trigger = out / 'start-sequence'
    env = os.environ.copy(); env['VTR_ENABLE_MARKER_TIMING'] = '0' if args.immediate else '1'
    log = logpath.open('w'); child = subprocess.Popen([str(args.host.resolve()), args.format, str(args.bundle.resolve()),
        str(recording), str(trigger), str(args.rate), str(args.block), '300'], stdout=log, stderr=log, env=env)
    session = None; competitor = None; competitor_log = None
    def refuse_competitor(label, owner_endpoint=None):
        nonlocal competitor, competitor_log
        kind = 'au' if args.format == 'vst3' else 'vst3'
        competitor_log = (out / (label + '.log')).open('w')
        competitor = subprocess.Popen([str(args.host.resolve()), kind, str(args.competitor_bundle.resolve()),
            str(out / (label + '.f32')), str(out / 'never-trigger'), str(args.rate), str(args.block), '30'],
            stdout=competitor_log, stderr=competitor_log, env=env)
        deadline = time.monotonic() + 10; other = None
        while time.monotonic() < deadline:
            matches = [s for s in discover() if s['pid'] == competitor.pid]
            if matches: other = matches[0]['socket']; break
            assert competitor.poll() is None; time.sleep(.1)
        assert other
        idle = request(other, 'status')
        assert not idle['connected'] and not idle['snapshot_directory']
        denied = request(other, 'connect'); target = denied['request_id']
        while denied['busy'] or denied['completed_request_id'] < target:
            assert time.monotonic() < deadline; time.sleep(.1); denied = request(other, 'status')
        assert not denied['last_command_succeeded'] and not denied['connected'] and not denied['device_lease_held']
        assert not denied['snapshot_directory'] and denied['lifetime_counters']['capture_callbacks'] == 0
        assert denied['lifetime_counters']['midi_sent'] == 0
        if owner_endpoint:
            still_owner = request(owner_endpoint, 'status')
            assert still_owner['connected'] and still_owner['device_lease_held']
            assert still_owner['snapshot_directory'] == str(session)
        competitor.terminate(); assert competitor.wait(timeout=15) == 0
        competitor_log.close(); competitor_log = None; competitor = None
    try:
        deadline = time.monotonic() + 15; endpoint = None
        while time.monotonic() < deadline:
            matches = [s for s in discover() if s['pid'] == child.pid]
            if matches: endpoint = matches[0]['socket']; break
            assert child.poll() is None, logpath.read_text(); time.sleep(.1)
        assert endpoint
        state = wait(endpoint, 'connect'); session = Path(state['snapshot_directory']); report['session'] = str(session); report['connected'] = state; persist()
        if args.immediate:
            assert not state['timing_enabled'] and state['timing_status'] != 'locked'
        else:
            assert state['timing_status'] == 'locked', 'No verified marker pattern; no test notes sent'
        wait(endpoint, {'command': 'load-file', 'path': str(args.patch.resolve()), 'preset': 0, 'part': 1})
        wait(endpoint, {'command': 'route', 'part': 1, 'bus': args.bus})
        # Patch upload is a potentially long SysEx operation. Do not send any
        # performance events until it has finished and the stream is stable.
        time.sleep(1); before = request(endpoint, 'status'); report['before'] = before; persist()
        trigger.touch(); sequence_deadline = time.monotonic() + 35
        if args.competitor_bundle:
            time.sleep(4)  # The finite sequence is sounding now.
            refuse_competitor('sounding-competitor', endpoint)
            report['sounding_competitor_refused_before_io'] = True; persist()
        time.sleep(max(0, sequence_deadline - time.monotonic()))
        after = request(endpoint, 'status'); report['after'] = after; persist()
        for counter in ('capture_errors', 'fifo_underruns', 'fifo_overruns'):
            assert after['lifetime_counters'][counter] == before['lifetime_counters'][counter], counter
        assert after['timing_late_events'] == before['timing_late_events'], 'Late stamped events'
        assert after['timing_overflow'] == before['timing_overflow'], 'Performance overflow'
        child.terminate(); assert child.wait(timeout=15) == 0
        if args.check_handoff:
            helper_state = json.loads((session / 'restoration-status.json').read_text())
            assert helper_state['state'] not in ('restored', 'failed'), 'Helper finished before contention check'
            # Probe the lease without any device command first. An unexpected
            # gap fails safely; never launch a competing Connect into that gap.
            with (Path.home() / 'Library/Application Support/VirusTIRework/hardware.lock').open('r+b') as lease:
                try: fcntl.flock(lease, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError: pass
                else: raise AssertionError('Exit helper did not retain the exclusive lease')
            refuse_competitor('handoff-competitor')
            report['helper_handoff_competitor_refused_before_io'] = True; persist()
        text = logpath.read_text(); assert 'note_on=256 note_off=256' in text, text
        first = int(re.search(r'FIXTURE_START (\d+)', text)[1])
        latency = before.get('reported_latency_samples', math.ceil((2048 + max(2 * math.ceil(args.block * 48000 / args.rate), 1024) + 32) * args.rate / 48000))
        report['measurement'] = analyze(recording, args.rate, first, latency, args.bus)
        if args.immediate:
            measurement = report['measurement']
            measurement['meets_synchronized_timing_limits'] = measurement['passed']
            measurement['acceptance_mode'] = 'immediate playback only; unsynchronized'
            measurement['passed'] = (measurement['detected_onsets'] == measurement['expected_notes']
                and measurement['release_tail_observed'] and measurement['release_tail_peak'] <= 1e-5
                and measurement['unselected_return_peak'] <= 1e-5)
        persist()
        assert report['measurement']['passed'], report['measurement']
    except BaseException as error:
        report['error'] = repr(error); persist(); raise
    finally:
        if session is None and endpoint is not None and child.poll() is None:
            try:
                recovery = request(endpoint, 'status').get('snapshot_directory')
                if recovery: session = Path(recovery); report['session'] = recovery
            except (OSError, ValueError, TimeoutError):
                pass
        if competitor is not None:
            if competitor.poll() is None: competitor.terminate()
            competitor.wait(timeout=20); competitor_log.close()
        if child.poll() is None: child.terminate()
        child.wait(timeout=20); log.close()
        if session:
            deadline = time.monotonic() + 150
            while time.monotonic() < deadline:
                status = json.loads((session / 'restoration-status.json').read_text())
                if status['state'] in ('restored', 'failed'): break
                time.sleep(.2)
            report['restoration_status'] = status
            try:
                assert status['state'] == 'restored', status
                report['restoration_audit'] = verify_restore(session, True)
                report['owner_protocol_audit'] = audit(session / 'private-out.bin', True)
                report['passed'] = report.get('measurement', {}).get('passed', False) and not report.get('error')
            except BaseException as error:
                report['passed'] = False; report['error'] = repr(error); persist(); raise
            persist()
    print(json.dumps(report['measurement'], indent=2))

if __name__ == '__main__': main()
