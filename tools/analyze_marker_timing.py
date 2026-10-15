#!/usr/bin/env python3
"""Analyze a private six-channel protocol_bench recording; never opens hardware.

Threshold onsets include patch attack/oscillator effects. Do not call them
sample-accurate proof or subtract a fitted offset to conceal latency error.
"""
import argparse
import array
import json
import math
from pathlib import Path
import statistics

def analyze(path, rate, first_frame, latency, bus=1, count=256):
    data = array.array('f', Path(path).read_bytes())
    assert len(data) % 6 == 0 and all(math.isfinite(v) for v in data)
    channel = (bus - 1) * 2
    peak = max((abs(v) for i, v in enumerate(data) if i % 6 in (channel, channel + 1)), default=0)
    assert peak > 1e-5, 'No usable audio on the requested return'
    high, low = max(1e-5, peak * .02), max(1e-6, peak * .005)
    quiet_needed = max(1, round(rate * .005)); quiet = quiet_needed
    onsets = []; last = -rate; armed = True
    for frame in range(len(data) // 6):
        level = max(abs(data[frame * 6 + channel]), abs(data[frame * 6 + channel + 1]))
        if level < low:
            quiet += 1
            if quiet >= quiet_needed: armed = True
        else:
            if armed and level >= high and frame - last > rate * .080:
                onsets.append(frame); last = frame; armed = False
            quiet = 0
    expected = [first_frame + round(i * rate * .125) + latency for i in range(count)]
    errors = [(a - b) / rate for a, b in zip(onsets, expected)]
    result = {'rate': rate, 'bus': bus, 'expected_notes': count, 'detected_onsets': len(onsets),
              'reported_latency_samples': latency, 'threshold_fraction': .02,
              'patch_effects_included': True, 'peak': peak, 'passed': False}
    if len(onsets) == count:
        x = [i * .125 for i in range(count)]; xm = statistics.mean(x); ym = statistics.mean(errors)
        slope = sum((a - xm) * (b - ym) for a, b in zip(x, errors)) / sum((a - xm) ** 2 for a in x)
        residual = [e - ym - slope * (t - xm) for t, e in zip(x, errors)]
        result.update(mean_error_ms=ym * 1000, jitter_sd_ms=statistics.pstdev(residual) * 1000,
                      drift_ppm=slope * 1e6, maximum_absolute_error_ms=max(map(abs, errors)) * 1000)
        result['passed'] = abs(result['mean_error_ms']) <= 1 and result['jitter_sd_ms'] <= .5 and abs(result['drift_ppm']) <= 10
    return result

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('recording', type=Path); p.add_argument('--rate', type=int, required=True)
    p.add_argument('--first-frame', type=int, required=True); p.add_argument('--latency', type=int, required=True)
    p.add_argument('--bus', type=int, choices=[1, 2, 3], default=1)
    args = p.parse_args()
    print(json.dumps(analyze(args.recording, args.rate, args.first_frame, args.latency, args.bus), indent=2))

if __name__ == '__main__': main()
