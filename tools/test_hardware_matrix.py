#!/usr/bin/env python3
"""Exercise a connected hardware test instance; preserve RAM and disconnect at exit.

Use a dedicated, actively processing vst3_api_host with six-channel float32 capture.
Pass its exact private socket, audio file and a fresh report directory. This script
does not discover instances, select firmware, write stored banks or drive a DAW UI.
The service's disconnect restores its pre-connect edit buffers and sends Single
mode, so the hardware should start in Single mode before the host connects.
"""
import argparse
import array
import ast
import hashlib
import json
import math
from pathlib import Path
import sys
import time

from test_multipart_api import checksums, decode, sound
from ti_control import request


COUNTERS = ('capture_callbacks', 'capture_errors', 'fifo_overruns', 'fifo_underruns',
            'rendered_frames', 'midi_sent')
FRAME_BYTES = 6 * 4


def require(condition, detail):
    if not condition:
        raise RuntimeError(str(detail))


def delta(before, after):
    return {k: after['lifetime_counters'][k] - before['lifetime_counters'][k]
            for k in COUNTERS}


def audio_stats(path, start, end):
    require(end > start and (end - start) % FRAME_BYTES == 0,
            'Audio did not advance by complete six-channel frames')
    with path.open('rb') as source:
        source.seek(start)
        raw = source.read(end - start)
    require(len(raw) == end - start, 'Audio capture was truncated')
    samples = array.array('f', raw)
    if sys.byteorder != 'little':
        samples.byteswap()
    peak, energy, derivative = [0.] * 6, [0.] * 6, [0.] * 6
    totals = [0.] * 6
    previous = None
    clipped = [0] * 6
    for at in range(0, len(samples), 6):
        frame = samples[at:at + 6]
        for channel, value in enumerate(frame):
            require(math.isfinite(value), 'Non-finite audio sample')
            peak[channel] = max(peak[channel], abs(value))
            energy[channel] += value * value
            totals[channel] += value
            clipped[channel] += int(abs(value) >= 1.)
            if previous is not None:
                derivative[channel] = max(derivative[channel], abs(value - previous[channel]))
        previous = frame
    frames = len(samples) // 6
    means = [value / frames for value in totals]
    return {'frames': frames, 'peak': peak, 'dc_mean': means,
            'ac_rms': [math.sqrt(max(0., energy[c] / frames - means[c] ** 2)) for c in range(6)],
            'rms': [math.sqrt(value / frames) for value in energy],
            'clipped_samples': clipped, 'max_sample_step': derivative}


class Matrix:
    def __init__(self, args):
        self.args = args
        self.out = args.output.resolve()
        self.out.mkdir(parents=True, exist_ok=True)
        require(not (self.out / 'report.json').exists()
                and not (self.out / 'initial-project.vtrstate').exists(),
                'Use a fresh output directory so initial-state evidence is never overwritten')
        self.report = {'socket': args.socket, 'audio': str(args.audio.resolve()),
                       'steps': [], 'auditions': [], 'stream_windows': [],
                       'success': False, 'cleanup': {},
                       'thresholds': {'noise_floor': args.noise_floor, 'signal_floor': args.signal_floor},
                       'scope': 'Edit buffers, MIDI and USB configuration; no firmware or stored-bank writes. '
                       'Hardware USB I/O is normalized to two returns on exit; globals are not snapshotted',
                       'audio_limitations': 'Sample steps and clipping are screening metrics, '
                       'not a conclusive click detector. Counter deltas detect reported '
                       'transport faults; listening remains necessary.',
                       'restoration_limitations': 'Service restores its pre-connect 16 edit '
                       'buffers, Single buffer and multi, then sends Single mode. Current '
                       'firmware patch-management metadata may differ; legacy patches may '
                       'undergo the documented firmware format conversion.'}
        self.pid = None
        self.initial = None
        self.held = set()
        self.identity = None
        self.noise = None
        self.steady = None

    def persist(self):
        temporary = self.out / 'report.tmp'
        temporary.write_text(json.dumps(self.report, indent=2) + '\n')
        temporary.replace(self.out / 'report.json')

    def status(self):
        state = request(self.args.socket, 'status')
        require(state.get('ok') and state.get('backend') == 'hardware', state)
        if self.pid is not None:
            require(state['pid'] == self.pid, 'Control instance changed during matrix')
        return state

    def run(self, command, timeout=180):
        started = time.monotonic()
        state = request(self.args.socket, command)
        require(state.get('ok'), state)
        request_id = state.get('request_id', 0)
        while (state.get('busy') or state.get('state_recall_pending')
               or state.get('completed_request_id', 0) < request_id):
            require(time.monotonic() - started < timeout, ('Command timeout', command, state))
            time.sleep(.05)
            state = self.status()
        require(state.get('backend') == 'hardware' and state['pid'] == self.pid, state)
        require(state.get('last_command_succeeded'), state)
        self.report['steps'].append({'command': command, 'seconds': time.monotonic() - started,
                                     'state': state})
        self.persist()
        print(command, 'OK', flush=True)
        return state

    def save(self, name):
        path = self.out / (name + '.vtrstate')
        self.run({'command': 'state-save', 'path': str(path)})
        return decode(path)

    def recall(self, name):
        return self.run({'command': 'state-load',
                         'path': str(self.out / (name + '.vtrstate'))})

    def position(self):
        info = self.args.audio.stat()
        identity = (info.st_dev, info.st_ino)
        require(self.identity is None or self.identity == identity, 'Capture file replaced')
        self.identity = identity
        return info.st_size // FRAME_BYTES * FRAME_BYTES

    def window(self, seconds):
        begin = self.position()
        time.sleep(seconds)
        end = self.position()
        result = audio_stats(self.args.audio, begin, end)
        result.update(start_byte=begin, end_byte=end)
        return result

    def settle(self, label, before):
        # Intentional stream startup/resets are kept separate from steady playback.
        time.sleep(2.)
        after = self.status()
        self.report['stream_windows'].append({'label': label, 'type': 'startup',
                                              'counters': delta(before, after)})
        self.steady = after
        self.persist()

    def end_steady(self, label):
        after = self.status()
        counters = delta(self.steady, after)
        self.report['stream_windows'].append({'label': label, 'type': 'steady',
                                              'counters': counters})
        self.persist()
        require(all(counters[k] == 0 for k in ('capture_errors', 'fifo_overruns', 'fifo_underruns')),
                ('Steady capture fault', label, counters))
        require(counters['capture_callbacks'] > 0 and counters['rendered_frames'] > 0,
                ('Stream stalled', label, counters))
        self.steady = after

    def note(self, part, note, on, velocity=28):
        key = (part, note)
        if on:
            self.held.add(key)  # Attempt the matching off even if the on times out.
        self.run({'command': 'note-on' if on else 'note-off', 'part': part,
                  'note': note, 'velocity': velocity})
        if not on:
            self.held.discard(key)

    def quiet(self, stats):
        for c in range(6):
            limit = max(self.args.noise_floor, self.noise['peak'][c] * 2.)
            require(stats['peak'][c] <= limit, ('Residual audio / possible stuck note', c, stats, limit))

    def audition(self, part, label):
        note = 48 + part
        begin = self.position()
        self.note(part, note, True)
        time.sleep(.4)
        self.note(part, note, False)
        time.sleep(1.)
        end = self.position()
        stats = audio_stats(self.args.audio, begin, end)
        tail_begin = max(begin, end - int(self.rate * .15) * FRAME_BYTES)
        tail = audio_stats(self.args.audio, tail_begin, end)
        bus = (part - 1) % 3
        item = {'phase': label, 'part': part, 'bus': bus + 1, 'start_byte': begin,
                'end_byte': end, 'audio': stats, 'release_tail': tail}
        self.report['auditions'].append(item)
        self.persist()
        require(max(stats['peak'][2 * bus:2 * bus + 2]) > self.args.signal_floor,
                ('No signal on selected USB return', item))
        for other in range(3):
            if other != bus:
                for c in (2 * other, 2 * other + 1):
                    require(stats['peak'][c] <= max(self.args.noise_floor, self.noise['peak'][c] * 2.),
                            ('Unexpected signal on another USB return', item))
        require(not any(stats['clipped_samples']), ('Clipping', item))
        self.quiet(tail)

    def identical(self, actual, expected):
        require(list(map(sound, actual[0])) == list(map(sound, expected[0])),
                'One or more recalled part sounds differ')
        for multi in (actual[1], expected[1]):
            require(len(multi) == 267 and multi[265] == sum(multi[5:265]) & 127,
                    'Invalid multi checksum')
        differences = [{'offset': i, 'expected': b, 'actual': a}
                       for i, (a, b) in enumerate(zip(actual[1], expected[1])) if a != b]
        require(all(d['offset'] in (10, 265) for d in differences),
                ('Recalled multi/routing differs', differences))
        if differences:
            self.report.setdefault('normalized_multi_metadata', []).append({
                'after_step': len(self.report['steps']), 'differences': differences,
                'sound_and_routing_identical': True})
            self.persist()

    def load_independent(self):
        fixture_path = Path(__file__).resolve().parents[1] / 'analysis/hardware/fixtures/stock-wavetable-probe.syx'
        template = fixture_path.read_bytes()
        require(len(template) == 524 and template[9] >= 12, 'Expected normalized stock RAM test patch')
        bank = bytearray()
        for part in range(16):
            patch = bytearray(template)
            patch[249:259] = f'MATRIX {part + 1:02d} '.encode('ascii')
            patch[26] = part * 8
            patch[100] = 32  # Reduced patch volume; especially important for 48-note chords.
            checksums(patch)
            bank.extend(patch)
        bank_path = self.out / 'sixteen-probes.syx'
        bank_path.write_bytes(bank)
        previous = self.initial
        for part in range(1, 17):
            self.run({'command': 'load-file', 'path': str(bank_path), 'preset': part - 1, 'part': part})
            actual = self.save(f'loaded-{part:02d}')
            p = actual[0][part - 1]
            require(p[249:259] == f'MATRIX {part:02d} '.encode() and p[26] == (part - 1) * 8
                    and p[100] == 32, ('Loaded patch readback differs', part))
            for other in range(16):
                if other != part - 1:
                    require(sound(actual[0][other]) == sound(previous[0][other]),
                            ('Loading changed a different part', part, other + 1))
            require(actual[1] == previous[1], 'Loading a single changed multi/routing')
            previous = actual
        self.report['independent_preset_loads'] = 16

    def resume_loads(self):
        source = self.args.resume_from.resolve()
        require(source != self.out, 'Resume source and output must differ')
        raw_report = (source / 'report.json').read_bytes()
        prior = json.loads(raw_report)
        require(prior.get('independent_preset_loads') == 16,
                'Resume source must contain all 16 original independent-load checks')
        require(prior.get('baseline', {}).get('backend') == 'hardware',
                'Cannot inherit emulator evidence as hardware evidence')
        cleanup = prior.get('cleanup', {})
        require(cleanup.get('initial_project_restored'),
                'Prior run must have finished restoring its initial project before resuming')
        if not (cleanup.get('hardware_originals_restored_by_service') and not cleanup.get('errors')):
            self.report['inherited_restoration_review'] = self.review_restoration(prior)
        # Recheck the source snapshots, rather than trusting a success field alone.
        previous = decode(source / 'initial-project.vtrstate')
        manifest = {}
        for name in ['initial-project'] + [f'loaded-{n:02d}' for n in range(1, 17)]:
            path = source / (name + '.vtrstate')
            manifest[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        for part in range(1, 17):
            actual = decode(source / f'loaded-{part:02d}.vtrstate')
            patch = actual[0][part - 1]
            require(patch[249:259] == f'MATRIX {part:02d} '.encode()
                    and patch[26] == (part - 1) * 8 and patch[100] == 32,
                    ('Invalid inherited probe', part))
            for other in range(16):
                if other != part - 1:
                    require(sound(actual[0][other]) == sound(previous[0][other]),
                            ('Invalid inherited independence evidence', part, other + 1))
            require(actual[1] == previous[1], 'Inherited single load changed multi')
            previous = actual
        raw_state = (source / 'all-parts.vtrstate').read_bytes()
        expected = decode(source / 'all-parts.vtrstate')
        require(expected[2] == 1, 'Inherited all-parts state must have reconnect intent')
        for part, patch in enumerate(expected[0]):
            require(patch[249:259] == f'MATRIX {part + 1:02d} '.encode()
                    and patch[26] == part * 8 and patch[100] == 32 and patch[49] == 81 + part,
                    ('Inherited all-parts probe differs', part + 1))
            require(expected[1][73 + part] == part
                    and expected[1][185 + part] == 10 + (part % 3) * 3,
                    ('Inherited channel / route differs', part + 1))
        (self.out / 'inherited-report.json').write_bytes(raw_report)
        (self.out / 'inherited-all-parts.vtrstate').write_bytes(raw_state)
        self.report['independent_preset_loads'] = 0
        self.report['inherited_independent_preset_loads'] = 16
        self.report['inherited_evidence'] = {
            'directory': str(source), 'report_sha256': hashlib.sha256(raw_report).hexdigest(),
            'state_sha256': hashlib.sha256(raw_state).hexdigest(), 'snapshot_sha256': manifest,
            'scope': 'Only independent preset-load evidence reused. All note/audio, live '
                     'cutoff/routing, state verification and reconnect checks rerun.'}
        self.persist()
        self.recall('inherited-all-parts')
        self.identical(self.save('imported-all-parts-readback'), expected)
        return expected

    @staticmethod
    def review_restoration(prior):
        """Accept only a proven metadata-only multi mismatch from the old service."""
        errors = prior.get('cleanup', {}).get('errors', [])
        prefix = 'disconnect / hardware restore: '
        require(len(errors) == 1 and errors[0].startswith(prefix),
                'Prior cleanup has additional or unrecognized errors')
        failure = ast.literal_eval(errors[0][len(prefix):])
        require(isinstance(failure, dict)
                and failure.get('status') == 'Disconnected · Restore needs attention: Multi restoration mismatch; '
                and failure.get('connected') is False and failure.get('busy') is False
                and failure.get('backend') == 'hardware'
                and failure.get('pid') == prior['baseline']['pid'],
                'Prior cleanup failure is not solely the known multi mismatch')
        directory = Path(prior['baseline']['snapshot_directory'])
        require(directory.is_absolute() and str(directory) == failure.get('snapshot_directory'),
                'Prior cleanup snapshots do not match its initial hardware session')
        review = {'snapshot_directory': str(directory), 'original_cleanup_error': errors[0],
                  'files': {}, 'parts': [],
                  'normalization': 'Multi Patchmanagement Flags at SysEx offset 10 and '
                                   'its checksum at offset 265 only; all sound/routing bytes identical.'}

        def read(name, size, command):
            path = directory / name
            data = path.read_bytes()
            require(len(data) == size and data[:5] == bytes.fromhex('f000203301')
                    and data[6] == command and data[-1] == 0xf7
                    and all(value < 128 for value in data[1:-1])
                    and data[-2] == sum(data[5:-2]) & 127,
                    ('Invalid restoration SysEx/checksum', name))
            if size == 524:
                require(data[265] == sum(data[5:265]) & 127,
                        ('Invalid restoration first-page checksum', name))
            review['files'][name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
            return data

        def differences(before, after):
            return [{'offset': i, 'before': a, 'after': b}
                    for i, (a, b) in enumerate(zip(before, after)) if a != b]

        for part in range(16):
            before = read(f'before-part{part}.syx', 524, 0x10)
            after = read(f'restored-part{part}.syx', 524, 0x10)
            require(before[8] == after[8] == part and sound(before) == sound(after),
                    ('Prior restored part sound differs', part + 1))
            review['parts'].append({'part': part + 1, 'sound_identical': True,
                                    'differences': differences(before, after)})
        before = read('before-single.syx', 524, 0x10)
        after = read('restored-single.syx', 524, 0x10)
        require(before == after, 'Prior Single buffer is not byte-identical')
        review['single_identical'] = True
        before = read('before-multi.syx', 267, 0x11)
        after = read('restored-multi.syx', 267, 0x11)
        diffs = differences(before, after)
        require(diffs and all(diff['offset'] in (10, 265) for diff in diffs)
                and any(diff['offset'] == 10 for diff in diffs),
                'Prior multi has a non-metadata difference or does not explain the failure')
        review['multi_differences'] = diffs
        review['multi_sound_routing_identical'] = True
        review['accepted_for_resume'] = True
        return review

    def perform(self):
        baseline = self.status()
        self.pid = baseline['pid']
        self.report['baseline'] = baseline
        self.persist()
        require(baseline['connected'] and baseline['host_processing'] and not baseline['busy'],
                'Connect a dedicated, actively processing hardware test instance first')
        require(not baseline['firmware_operations_supported']
                and not baseline['stored_bank_writes_supported'], 'Unexpected API write capabilities')
        self.rate = int(baseline['host_sample_rate'])
        require(self.rate in (44100, 48000, 96000), 'Unexpected host capture sample rate')
        self.position()
        self.initial = self.save('initial-project')
        require(self.initial[2] == 1, 'Expected a connected initial project')
        self.settle('initial connection already established', baseline)

        if self.args.resume_from:
            imported = self.resume_loads()
        else:
            self.load_independent()
            imported = None
        for part in range(1, 17):
            self.run({'command': 'cutoff', 'part': part, 'value': 80 + part})
            self.run({'command': 'route', 'part': part, 'bus': (part - 1) % 3 + 1})
        expected = self.save('all-parts')
        if imported is not None:
            self.identical(expected, imported)
        for p in range(16):
            require(expected[0][p][49] == 81 + p, ('Live cutoff edit not captured', p + 1))
            require(expected[1][73 + p] == p and expected[1][185 + p] == 10 + (p % 3) * 3,
                    ('MIDI channel / USB routing readback differs', p + 1))
        time.sleep(1.)
        self.noise = self.window(.4)
        self.report['silence_baseline'] = self.noise
        require(max(self.noise['peak']) <= self.args.noise_floor,
                ('Noise baseline too high / unrelated notes active', self.noise))
        for part in range(1, 17):
            self.audition(part, 'initial')
        begin = self.position()
        for part in range(1, 17):
            for note in (48, 52, 55):
                self.note(part, note, True, velocity=20)
        time.sleep(.8)
        for part, note in sorted(self.held.copy()):
            self.note(part, note, False)
        time.sleep(1.)
        end = self.position()
        chord = audio_stats(self.args.audio, begin, end)
        tail = audio_stats(self.args.audio, end - int(self.rate * .15) * FRAME_BYTES, end)
        self.report['concurrent_chords'] = {'audio': chord, 'release_tail': tail,
                                             'start_byte': begin, 'end_byte': end, 'notes': 48}
        self.persist()
        require(all(max(chord['peak'][2 * b:2 * b + 2]) > self.args.signal_floor for b in range(3)),
                ('Concurrent chords missing a USB bus', chord))
        require(not any(chord['clipped_samples']), ('Concurrent chord clipping', chord))
        self.quiet(tail)

        for part in range(1, 17):
            self.run({'command': 'cutoff', 'part': part, 'value': 25})
            self.run({'command': 'route', 'part': part, 'bus': 1})
        changed = self.save('changed')
        require(all(sound(a) != sound(b) for a, b in zip(changed[0], expected[0]))
                and changed[1] != expected[1], 'Recall challenge did not change all parts and routing')
        self.recall('all-parts')
        self.identical(self.save('recalled'), expected)
        invalid = bytearray((self.out / 'all-parts.vtrstate').read_bytes())
        invalid[-1] ^= 1
        invalid_path = self.out / 'bad-multi-crc.vtrstate'
        invalid_path.write_bytes(invalid)
        rejection = request(self.args.socket, {'command': 'state-load', 'path': str(invalid_path)})
        self.report['bad_crc_rejection'] = rejection
        require(not rejection['ok'] and rejection['connected'], 'Corrupt state was not rejected atomically')
        self.identical(self.save('after-rejected'), expected)
        self.end_steady('load, routing, isolated notes, chords and recall')

        for cycle in (1, 2):
            before = self.status()
            disconnected = self.run('disconnect')
            require(not disconnected['connected'] and 'needs attention' not in disconnected['status'].lower(),
                    ('Hardware restoration failed', disconnected))
            offline = self.save(f'offline-{cycle}')
            require(offline[2] == 0, 'Offline state retains automatic reconnect intent')
            self.identical(offline, expected)
            self.recall('all-parts')
            self.settle(f'reconnect {cycle}', before)
            self.identical(self.save(f'after-reconnect-{cycle}'), expected)
            for part in range(1, 17):
                self.audition(part, f'reconnect-{cycle}')
            self.end_steady(f'reconnect {cycle} all 16 parts')
        self.report['matrix_complete'] = True

    def cleanup(self):
        if self.initial is None:
            return  # No matrix mutations occurred before a valid initial snapshot.
        errors = []
        try:
            state = self.status()
            if state['connected']:
                for part, note in sorted(self.held.copy()):
                    try:
                        self.note(part, note, False)
                    except Exception as error:
                        errors.append(f'note-off {part}/{note}: {error}')
            self.recall('initial-project')
            self.identical(self.save('restored-initial-project'), self.initial)
            self.report['cleanup']['initial_project_restored'] = True
        except Exception as error:
            errors.append(f'initial project restore: {error}')
        finally:
            try:
                state = self.run('disconnect')
                require(not state['connected'] and 'needs attention' not in state['status'].lower(), state)
                self.report['cleanup']['hardware_originals_restored_by_service'] = True
                self.report['cleanup']['final_state'] = state
            except Exception as error:
                errors.append(f'disconnect / hardware restore: {error}')
        self.report['cleanup']['errors'] = errors
        self.persist()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--socket', required=True, help='Exact connected hardware test-instance socket')
    parser.add_argument('--audio', required=True, type=Path, help='Growing six-channel little-endian float32 capture')
    parser.add_argument('--output', required=True, type=Path, help='Fresh evidence directory')
    parser.add_argument('--resume-from', type=Path, help='Prior hardware report directory with completed independent loads and successful cleanup; remaining tests rerun')
    parser.add_argument('--noise-floor', type=float, default=1e-4, help='Maximum idle/unrouted peak (default about -80 dBFS)')
    parser.add_argument('--signal-floor', type=float, default=5e-4, help='Minimum sounding-bus peak')
    args = parser.parse_args()
    require(0 < args.noise_floor < args.signal_floor < .1, 'Expected 0 < noise floor < signal floor < 0.1')
    matrix = Matrix(args)
    try:
        matrix.perform()
    except (Exception, KeyboardInterrupt) as error:
        matrix.report['error'] = f'{type(error).__name__}: {error}'
        print(matrix.report['error'], file=sys.stderr, flush=True)
    finally:
        matrix.cleanup()
        matrix.report['success'] = (matrix.report.get('matrix_complete', False)
                                    and not matrix.report['cleanup'].get('errors')
                                    and matrix.report['cleanup'].get('hardware_originals_restored_by_service', False))
        matrix.persist()
    print('PASS' if matrix.report['success'] else 'FAIL', str(matrix.out / 'report.json'), flush=True)
    return 0 if matrix.report['success'] else 1


if __name__ == '__main__':
    sys.exit(main())
