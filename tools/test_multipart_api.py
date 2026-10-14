#!/usr/bin/env python3
"""Test the real plug-in API and state callbacks against the full OsTIrus TI backend."""
import argparse
import array
import json
import math
from pathlib import Path
import struct
import time
import zlib
from ti_control import request


def decode(path):
    data = Path(path).read_bytes()
    magic, version, count = struct.unpack_from('<III', data)
    assert (magic, version, count) == (0x56545257, 3, 2672)
    at = 12 + count * 12
    flags, count = struct.unpack_from('<II', data, at); at += 8
    assert count == 16 and flags in (0, 1)
    blobs = []
    for n in range(17):
        size, = struct.unpack_from('<I', data, at); at += 4
        patch = data[at:at+size]; at += size
        crc, = struct.unpack_from('<I', data, at); at += 4
        assert crc == zlib.crc32(patch)
        assert size == (524 if n < 16 else 267)
        if n < 16:
            assert patch[8] == n
        blobs.append(patch)
    assert at == len(data)
    return blobs[:16], blobs[16], flags


def sound(p):
    return bytes(b for i, b in enumerate(p) if i not in (5, 7, 8, 10, 11, 12, 265, 522))


def checksums(p):
    p[265] = sum(p[5:265]) & 127
    p[522] = sum(p[5:522]) & 127


def audio_stats(path, start, end):
    with open(path, 'rb') as f:
        f.seek(start); values = array.array('f', f.read(end-start))
    peaks = [0.] * 6; sums = [0.] * 6
    for i, value in enumerate(values):
        assert math.isfinite(value)
        c = i % 6; peaks[c] = max(peaks[c], abs(value)); sums[c] += value * value
    return {'peaks': peaks, 'rms': [math.sqrt(v / (len(values)//6)) for v in sums], 'frames': len(values)//6}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--socket', required=True)
    ap.add_argument('--audio', type=Path, required=True)
    ap.add_argument('--output', type=Path, default=Path('analysis/daw-validation/multipart'))
    ap.add_argument('--restart', action='store_true')
    args = ap.parse_args(); out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    endpoint = args.socket; report = {'socket': endpoint, 'steps': []}
    phase = 'restart' if args.restart else 'roundtrip'

    def persist():
        (out / (phase + '.json')).write_text(json.dumps(report, indent=2))

    def run(command):
        start = time.monotonic(); state = request(endpoint, command)
        assert state['ok'], state
        request_id = state.get('request_id', 0)
        while state['busy'] or state.get('state_recall_pending') or state['completed_request_id'] < request_id:
            assert time.monotonic() - start < 100, state
            time.sleep(.05); state = request(endpoint, 'status')
        assert state['last_command_succeeded'], state
        assert state['backend'] == 'emulator'
        report['steps'].append({'command': command, 'seconds': time.monotonic()-start, 'state': state})
        persist(); print(command, 'OK', flush=True)
        return state

    def save(name):
        path = out / (name + '.vtrstate'); run({'command': 'state-save', 'path': str(path)})
        return decode(path)

    def recall(name):
        return run({'command': 'state-load', 'path': str(out / (name + '.vtrstate'))})

    report['baseline'] = request(endpoint, 'status')
    assert report['baseline']['backend'] == 'emulator'
    if args.restart:
        assert not report['baseline']['connected']
        assert report['baseline']['pid'] != json.loads((out/'roundtrip.json').read_text())['baseline']['pid']
        expected, multi, _ = decode(out/'all-parts.vtrstate')
        recall('all-parts')
        actual, restored_multi, _ = save('after-restart')
        assert list(map(sound, actual)) == list(map(sound, expected))
        assert restored_multi == multi
        report['all_16_sounds_and_multi_restored_in_fresh_process'] = True
        # Data recall must also produce audible, correctly routed voices after reboot.
        time.sleep(.5)
        report['playback_baseline'] = request(endpoint, 'status')
        report['auditions'] = []
        for part in range(1, 17):
            start = args.audio.stat().st_size // 24 * 24
            run({'command': 'note-on', 'part': part, 'note': 48+part, 'velocity': 35})
            time.sleep(.35)
            run({'command': 'note-off', 'part': part, 'note': 48+part})
            time.sleep(.65)
            end = args.audio.stat().st_size // 24 * 24
            stats = audio_stats(args.audio, start, end); bus = (part-1) % 3
            assert max(stats['peaks'][bus*2:bus*2+2]) > .001, (part, stats)
            for other in range(3):
                if other != bus: assert max(stats['peaks'][other*2:other*2+2]) < 1e-5, (part, stats)
            tail = audio_stats(args.audio, end-4800*24, end)
            assert max(tail['peaks']) < 1e-5, (part, tail)
            report['auditions'].append({'part': part, 'bus': bus+1, 'start_byte': start,
                                        'end_byte': end, 'release_tail': tail, **stats})
            persist()
        playback_end = request(endpoint, 'status')
        for counter in ('fifo_underruns', 'fifo_overruns', 'capture_errors'):
            assert playback_end['lifetime_counters'][counter] == report['playback_baseline']['lifetime_counters'][counter]
        report['all_16_recalled_parts_play_and_release'] = True
    else:
        assert report['baseline']['connected']
        previous, multi, _ = save('baseline')
        template = Path('analysis/hardware/fixtures/stock-wavetable-probe.syx').read_bytes()
        fixture = bytearray()
        for part in range(16):
            p = bytearray(template); p[249:259] = f'PART {part+1:02d}   '.encode(); p[26] = part * 8
            checksums(p); fixture += p
        bank = out/'sixteen-parts.syx'; bank.write_bytes(fixture)
        for part in range(1, 17):
            run({'command': 'load-file', 'path': str(bank), 'preset': part-1, 'part': part})
            actual, current_multi, _ = save(f'loaded-{part:02d}')
            assert actual[part-1][249:259] == f'PART {part:02d}   '.encode()
            assert actual[part-1][26] == (part-1)*8
            for other in range(16):
                if other != part-1: assert sound(actual[other]) == sound(previous[other]), (part, other)
            assert current_multi == multi
            previous = actual
        report['independent_preset_loads'] = 16
        # MIDI edits and all three USB-return routes, independently addressed.
        for part in range(1,17):
            run({'command': 'cutoff', 'value': 80+part, 'part': part})
            run({'command': 'route', 'bus': (part-1)%3+1, 'part': part})
        expected, multi, _ = save('all-parts')
        for p in range(16):
            assert expected[p][49] == 81+p
            assert multi[73+p] == p
            assert multi[185+p] == 10+(p%3)*3
        # Each channel must produce audio only on its selected stereo bus.
        report['auditions'] = []
        time.sleep(.3)
        for part in range(1,17):
            start = args.audio.stat().st_size // 24 * 24
            run({'command':'note-on','part':part,'note':48+part,'velocity':35})
            time.sleep(.35)
            run({'command':'note-off','part':part,'note':48+part})
            time.sleep(.4)
            end = args.audio.stat().st_size // 24 * 24
            stats = audio_stats(args.audio,start,end); bus=(part-1)%3
            assert max(stats['peaks'][bus*2:bus*2+2]) > .001, (part,stats)
            for other in range(3):
                if other != bus: assert max(stats['peaks'][other*2:other*2+2]) < 1e-5, (part,stats)
            report['auditions'].append({'part':part,'bus':bus+1,'start_byte':start,'end_byte':end,**stats});persist()
        # All 16 parts active together, then all releases.
        start=args.audio.stat().st_size//24*24
        for part in range(1,17):run({'command':'note-on','part':part,'note':48+(part%12),'velocity':25})
        time.sleep(.5)
        for part in range(1,17):run({'command':'note-off','part':part,'note':48+(part%12)})
        time.sleep(.5);end=args.audio.stat().st_size//24*24
        report['sixteen_part_chord']=audio_stats(args.audio,start,end)
        assert all(max(report['sixteen_part_chord']['peaks'][b*2:b*2+2])>.001 for b in range(3))
        assert max(report['sixteen_part_chord']['peaks'])<1
        # Change every patch and route, then recall all original parts + multi.
        for part in range(1,17):
            run({'command':'cutoff','part':part,'value':25})
            run({'command':'route','part':part,'bus':1})
        changed, changed_multi, _=save('changed')
        assert all(sound(a)!=sound(b) for a,b in zip(changed,expected))
        assert changed_multi!=multi
        recall('all-parts');actual,actual_multi,_=save('recalled')
        assert list(map(sound,actual))==list(map(sound,expected));assert actual_multi==multi
        report['all_16_sounds_and_multi_restored']=True
        # Invalid late-part state must never partially apply early parts.
        raw=bytearray((out/'all-parts.vtrstate').read_bytes());raw[-1]^=1
        invalid=out/'bad-multi-crc.vtrstate';invalid.write_bytes(raw)
        rejected=request(endpoint,{'command':'state-load','path':str(invalid)})
        assert not rejected['ok'] and rejected['connected']
        after,after_multi,_=save('after-rejected');assert list(map(sound,after))==list(map(sound,expected));assert after_multi==multi
        run('disconnect');offline,offline_multi,flags=save('offline')
        assert flags==0 and list(map(sound,offline))==list(map(sound,expected)) and offline_multi==multi
        recall('all-parts');again,again_multi,_=save('after-reconnect')
        assert list(map(sound,again))==list(map(sound,expected));assert again_multi==multi
        report['disconnect_reconnect_all_parts']=True
    report['final']=request(endpoint,'status');report['success']=True;persist()
    print('PASS',phase,flush=True)


if __name__=='__main__':main()
