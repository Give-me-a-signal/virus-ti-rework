#!/usr/bin/env python3
"""Disassemble native 24-bit BE DSP56300 words using pinned upstream CLI.

--base is the DSP word address of byte zero; --offset selects a word offset.
Output is a linear sweep: interleaved data can decode as valid instructions.
"""
from pathlib import Path
import argparse
import subprocess


def number(value):
    return int(value, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--base', type=number, default=0)
    parser.add_argument('--offset', type=number, default=0)
    parser.add_argument('--count', type=number)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    executable = root / 'third_party/dsp56300-rust/target/release/dsp56300-disasm'
    data = args.input.read_bytes()
    if len(data) % 3:
        parser.error('input size is not divisible by three bytes per DSP word')
    if not 0 <= args.offset <= len(data) // 3:
        parser.error('word offset is outside input')
    command = [str(executable), '-f', 'u24be', '--color', 'never', '-s',
               hex(args.base + args.offset)]
    if args.count is not None:
        command += ['-n', str(args.count)]
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('wb') as output:
            subprocess.run(command, input=data[args.offset * 3:], stdout=output, check=True)
    else:
        subprocess.run(command, input=data[args.offset * 3:], check=True)


if __name__ == '__main__':
    main()
