#!/usr/bin/env python3
"""Export recovered signed-24 cycles losslessly and make a derived listening preview."""
import csv
import json
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'analysis/wavetable-trace/replay'


def write_wave(path, samples, rate):
    with wave.open(str(path), 'wb') as output:
        output.setnchannels(1)
        output.setsampwidth(3)
        output.setframerate(rate)
        output.writeframes(b''.join(int(v).to_bytes(3, 'little', signed=True) for v in samples))


def main():
    for variant in ('original', 'reversed'):
        folder = BASE / variant
        rows = list(csv.reader((folder / 'frames-12x64-signed24.csv').open()))
        frames = [[int(v) for v in row[1:]] for row in rows[1:]]
        assert len(frames) == 12 and all(len(f) == 64 for f in frames)
        raw = [v for frame in frames for v in frame]
        cycle_path = folder / 'HarmncSweep-12x64.wav'
        write_wave(cycle_path, raw, 48000)
        # The stored cycles start half a cycle before the descriptor's phase-zero
        # pointer. Preserve that cache order in the lossless export and preview.
        rate, duration, pitch = 48000, 6, 220
        preview = []
        for n in range(rate * duration):
            position = n / (rate * duration - 1) * 11
            a = min(int(position), 10)
            blend = position - a
            phase = (n * pitch / rate * 64) % 64
            s, fraction = int(phase), phase % 1
            def sample(frame):
                return frame[s] * (1-fraction) + frame[(s+1) % 64] * fraction
            value = sample(frames[a]) * (1-blend) + sample(frames[a+1]) * blend
            fade = min(1, n / (rate*.02), (rate*duration-1-n) / (rate*.02))
            preview.append(round(value * fade))
        write_wave(folder / 'HarmncSweep-sweep-preview.wav', preview, rate)
        # Confirm the lossless export retains all 768 original integer samples.
        with wave.open(str(cycle_path), 'rb') as source:
            data = source.readframes(source.getnframes())
            assert source.getnframes() == 768 and source.getsampwidth() == 3
        assert [int.from_bytes(data[i:i+3], 'little', signed=True) for i in range(0, len(data), 3)] == raw
        (folder / 'audio-export.json').write_text(json.dumps({
            'cycles': 12, 'samples_per_cycle': 64, 'sample_format': 'signed 24-bit PCM',
            'lossless_cycle_export_verified': True,
            'preview': 'Derived 6-second, 220 Hz sweep with linear phase/frame interpolation and 20 ms fades; not a hardware or complete oscillator render',
            'cycle_order': 'Cache order; descriptor phase-zero pointer is at sample 32'
        }, indent=2) + '\n')
        print(variant, 'verified 768 lossless samples; wrote 6-second preview')


if __name__ == '__main__':
    main()
