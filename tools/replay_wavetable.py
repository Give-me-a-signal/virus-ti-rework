#!/usr/bin/env python3
"""Identify all replay inputs, then execute isolated firmware routines in DSP emulation."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import csv
import re

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--load-dir', type=Path, required=True)
parser.add_argument('--config', type=Path, required=True)
parser.add_argument('--image', type=Path, required=True)
parser.add_argument('--output-dir', type=Path, help='Save complete replay output and export ID 1 rendered frames; print a compact summary')
args = parser.parse_args()
executable = root / 'native/dsp-probe/target/release/dsp-probe'
paths = [executable, args.config, args.image] + sorted(args.load_dir.glob('*.24be.bin'))
print('REPLAY_INPUTS ' + json.dumps([
    {'path': str(p.resolve()), 'size': p.stat().st_size, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
    for p in paths
]), flush=True)
print('REPLAY_SCOPE isolated firmware routines in emulated DSP; allocator/profiler stubs; no synth boot or hardware I/O', flush=True)
command = [str(executable), str(args.load_dir.resolve()), str(args.config.resolve())]
if args.output_dir is None:
    subprocess.run(command, check=True)
else:
    run = subprocess.run(command, check=True, capture_output=True, text=True)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / 'replay-output.txt').write_text(run.stdout)
    memory = {}
    for space, address, value in re.findall(r'^MEM ([XYP]) ([0-9a-f]+) ([0-9a-f]+)$', run.stdout, re.M):
        memory[(space, int(address, 16))] = int(value, 16)
    base = memory[('X', 0x5ff2e)]
    rendered = [memory[('X', base + i)] for i in range(0xdf5)]
    assert rendered[4] == 294 and rendered[2] + 1 == 12
    binary = b''.join(v.to_bytes(3, 'big') for v in rendered)
    (args.output_dir / 'rendered-table.24be.bin').write_bytes(binary)
    frame_words = [rendered[45 + i*294:45 + (i+1)*294] for i in range(12)]
    levels = [64, 64, 64, 32, 32, 32]
    guards = []
    for frame in frame_words:
        offset = 0
        for length in levels:
            guards.append(frame[offset] == frame[offset + length])
            offset += length + 1
        assert offset == 294
    assert all(guards)
    with (args.output_dir / 'frames-12x64-signed24.csv').open('w') as f:
        writer = csv.writer(f)
        writer.writerow(['frame'] + [f'sample_{i}' for i in range(64)])
        for i, frame in enumerate(frame_words):
            writer.writerow([i] + [v - (1 << 24) if v & 0x800000 else v for v in frame[:64]])
    summary = {'stop': next(l for l in run.stdout.splitlines() if l.startswith('STOP ')),
               'cache_word_address': base, 'rendered_words': len(rendered),
               'frames': 12, 'frame_stride_words': 294, 'level_lengths': levels,
               'all_72_cycle_guards_match': all(guards),
               'rendered_sha256': hashlib.sha256(binary).hexdigest(),
               'output_directory': str(args.output_dir.resolve()),
               'scope': 'Actual firmware routines executed in isolated emulation; not a full synth or hardware test'}
    (args.output_dir / 'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print('REPLAY_RESULT ' + json.dumps(summary))
