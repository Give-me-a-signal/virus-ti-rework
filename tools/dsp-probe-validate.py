#!/usr/bin/env python3
"""Verify independent ID1 coefficient math against isolated firmware replay."""
from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LOAD = ROOT / 'analysis/firmware-images/decoded/vti/dsp-load'
RUNNER = ROOT / 'native/dsp-probe/target/release/dsp-probe'


def main():
    config = ROOT / 'analysis/dsp-probe-unpack-id1.txt'
    output = subprocess.check_output([str(RUNNER), str(LOAD), str(config)], text=True)
    config.with_suffix('.out').write_text(output)
    actual = [int(line.split()[3], 16) for line in output.splitlines()
              if line.startswith('MEM X')]
    image = (LOAD / '15_X_04f000.24be.bin').read_bytes()
    def word(address):
        offset = (address - 0x4f000) * 3
        return int.from_bytes(image[offset:offset + 3], 'big')
    pointer = word(0x5ff2d)
    exponent, frames, _, gain = [word(pointer + i) for i in range(4)]
    count = (1 << exponent) * frames
    offset = (pointer + 4 - 0x4f000) * 3
    encoded = image[offset:offset + count]
    decoded_gain = ((gain * gain >> 23) * 0x517cc2) >> 24
    predicted = []
    for raw in encoded:
        signed = raw if raw < 128 else raw - 256
        squared = min(signed * signed << 9, 0x7fffff)
        predicted.append(squared * decoded_gain >> 23)
    result = dict(gain_header=hex(gain), decoded_gain=hex(decoded_gain),
                  coefficient_count=len(actual),
                  mismatches=sum(a != b for a, b in zip(actual, predicted)),
                  exact_match=actual == predicted, first_values=actual[:36])
    (ROOT / 'analysis/dsp-probe-coeff-validation.json').write_text(
        json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    assert actual == predicted, 'replay disagrees with independent coefficient formula'


if __name__ == '__main__':
    main()
