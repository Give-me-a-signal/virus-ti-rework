#!/usr/bin/env python3
"""Summarize completed 16-part emulator API runs and export the recalled auditions."""
import array
from collections import Counter
import hashlib
import json
from pathlib import Path
import shutil
import wave

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'analysis/daw-validation/multipart'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    runs = {name: json.loads((OUT / f'{name}.json').read_text()) for name in ('roundtrip', 'restart')}
    assert all(r['success'] and r['final']['backend'] == 'emulator' for r in runs.values())
    assert runs['roundtrip']['baseline']['pid'] != runs['restart']['baseline']['pid']
    sessions = sorted({s['state']['snapshot_directory'] for r in runs.values() for s in r['steps']
                       if s['state']['snapshot_directory']})
    audit = []
    for directory in sessions:
        source = Path(directory)
        target = OUT / 'sessions' / source.name
        shutil.copytree(source, target, dirs_exist_ok=True)
        counts = Counter(); messages = 0
        for line in (target / 'emulator-midi-out.txt').read_text().splitlines():
            p = bytes(int(v, 16) for v in line.split()); messages += 1
            if p[0] == 0xf0:
                assert p[:5] == bytes.fromhex('f0 00 20 33 01') and p[-1] == 0xf7
                counts[f'{p[6]:02x}'] += 1
                assert p[6] in (0x10, 0x11, 0x30, 0x31, 0x72, 0x73)
                if p[6] == 0x10: assert p[7] == 0 and p[8] < 16
                if p[6] == 0x11: assert p[7:9] == bytes((0, 127))
            else:
                assert len(p) == 3 and p[0] & 0xf0 in (0x80, 0x90, 0xb0)
        before = (target / 'before-multi.syx').read_bytes()
        assert before == (target / 'restored-multi.syx').read_bytes()
        assert (target / 'before-single.syx').read_bytes() == (target / 'restored-single.syx').read_bytes()
        audit.append({'directory': str(target.relative_to(ROOT)), 'messages': messages,
                      'sysex_commands_hex': dict(counts), 'single_and_multi_restored_exactly': True,
                      'legacy_part_conversions': (target / 'session.log').read_text().count('running-firmware format conversion')})

    # One short stereo demonstration: sum the three stereo returns, without normalization.
    audio = OUT / 'audio-verified-restart.f32'
    output = OUT / 'sixteen-recalled-parts.wav'
    total_frames = 0; peak = 0.
    with audio.open('rb') as src, wave.open(str(output), 'wb') as dest:
        dest.setnchannels(2); dest.setsampwidth(3); dest.setframerate(48000)
        for item in runs['restart']['auditions']:
            src.seek(item['start_byte']); samples = array.array('f', src.read(item['end_byte'] - item['start_byte']))
            pcm = bytearray()
            for f in range(len(samples)//6):
                for c in (0, 1):
                    value = sum(samples[f*6+c+2*b] for b in range(3)); peak = max(peak, abs(value))
                    assert abs(value) < 1
                    pcm.extend(int(round(value*8388607)).to_bytes(3, 'little', signed=True))
            total_frames += len(samples)//6; dest.writeframes(pcm)

    summary = {
        'date': '2026-10-11', 'backend': 'full OsTIrus TI emulator',
        'independent_preset_loads': runs['roundtrip']['independent_preset_loads'],
        'full_project_state_bytes': (OUT / 'all-parts.vtrstate').stat().st_size,
        'all_16_sounds_and_multi_restored': runs['roundtrip']['all_16_sounds_and_multi_restored'],
        'disconnect_reconnect_all_parts': runs['roundtrip']['disconnect_reconnect_all_parts'],
        'fresh_process_recall_and_audio': runs['restart']['all_16_recalled_parts_play_and_release'],
        'roundtrip_counters': runs['roundtrip']['final']['lifetime_counters'],
        'restart_counters': runs['restart']['final']['lifetime_counters'],
        'restart_playback_counter_deltas': {k: runs['restart']['final']['lifetime_counters'][k] - runs['restart']['playback_baseline']['lifetime_counters'][k]
                                           for k in ('fifo_overruns', 'fifo_underruns', 'capture_errors')},
        'sixteen_part_chord': runs['roundtrip']['sixteen_part_chord'],
        'largest_release_tail_peak': max(max(a['release_tail']['peaks']) for a in runs['restart']['auditions']),
        'midi_audit': audit, 'firmware_transfers': 0, 'stored_bank_writes': 0,
        'demo': {'path': str(output.relative_to(ROOT)), 'seconds': total_frames/48000, 'peak': peak},
        'artifacts_sha256': {},
        'limits': ['Emulator tests do not validate physical USB or parts 2–16 on the TI.',
                   'Live licence blocks actual .als saving; this tests the real processor state callbacks.',
                   'New build is not substituted for the older hardware build currently loaded in Live.',
                   'Backend/ROM selection is explicit per instance and not stored in project chunks.',
                   'Legacy emulator factory buffers are firmware-converted on restoration, not byte-identical.']}
    for p in [ROOT/'native/vst3/build-multipart/vst3_api_host',
              ROOT/'native/vst3/build-multipart/VST3/Release/Virus TI Rework Development.vst3/Contents/MacOS/Virus TI Rework Development',
              ROOT/'native/osti-render/build/libosti-backend.dylib', OUT/'all-parts.vtrstate', output]:
        summary['artifacts_sha256'][str(p.relative_to(ROOT))] = digest(p)
    (OUT/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
