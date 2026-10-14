#!/usr/bin/env python3
"""Probe ID1 spectral bases through original firmware instructions, offline."""
import cmath
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'analysis/wavetable-trace/wav-import/basis'
LOAD = ROOT / 'analysis/firmware-images/decoded/vti/dsp-load'
RUNNER = ROOT / 'native/dsp-probe/target/release/dsp-probe'


def spectrum(samples):
    n = len(samples)
    return [sum(v*cmath.exp(-2j*math.pi*k*i/n) for i, v in enumerate(samples))/n
            for k in range(n//2+1)]


def probe(name, frames):
    assert len(frames) == 12 and all(len(f) == 32 for f in frames)
    packed = bytes(q for f in frames for q in f)
    config = (ROOT / 'analysis/dsp-probe-render-id1.txt').read_text()
    config += ''.join(f'mem X {0x5fff7+i//3:x} {int.from_bytes(packed[i:i+3], "big"):x}\n'
                      for i in range(0, len(packed), 3))
    path = OUT / (name + '.txt')
    path.write_text(config)
    run = subprocess.run([str(RUNNER), str(LOAD), str(path)], check=True,
                         capture_output=True, text=True)
    (OUT / (name + '.out')).write_text(run.stdout)
    memory = {int(a,16): int(v,16) for a,v in re.findall(r'^MEM X ([0-9a-f]+) ([0-9a-f]+)$',run.stdout,re.M)}
    base = memory[0x5ff2e]
    cache = [memory[base+i] for i in range(3573)]
    result = []
    for f in range(12):
        raw = cache[45+f*294:45+(f+1)*294]
        signed = [v-(1<<24) if v & 0x800000 else v for v in raw]
        spec = spectrum(signed[:64])
        dominant = max(range(1,33), key=lambda k: abs(spec[k]))
        result.append({'input':frames[f], 'samples':signed[:64], 'all_levels':signed,
                       'dominant_harmonic':dominant, 'amplitude':2*abs(spec[dominant]),
                       'phase_radians':cmath.phase(spec[dominant]),
                       'spectrum_complex':[[z.real,z.imag] for z in spec]})
    return result


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    provenance = {'runner_sha256': hashlib.sha256(RUNNER.read_bytes()).hexdigest(),
                  'load_block_hashes': {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(LOAD.glob('*.24be.bin'))}}
    bases = []
    for start in (0,12,24):
        frames = [[80 if k == (start+i)%32 else 0 for k in range(32)] for i in range(12)]
        bases += probe(f'basis-{start:02}', frames)[:min(12,32-start)]
    amplitudes = probe('amplitudes', [[q if k==0 else 0 for k in range(32)] for q in (0,1,2,4,8,16,32,64,80,100,120,127)])
    mixtures = probe('mixtures', [[q if k in bins else 0 for k in range(32)] for q,bins in (
        (20,[0,1]),(40,[0,1]),(80,[0,1]),(127,[0,1]),
        (20,[0,2,6]),(80,[0,2,6]),(127,[0,2,6]),
        (10,list(range(32))),(40,list(range(32))),(80,list(range(32))),(127,list(range(32))),(0,[]))])
    result = {'provenance':provenance,'scope':'ID1, flag0, original gain, controlled allocator/timer and normal SR; memory coefficient overrides',
              'basis':bases,'amplitudes':amplitudes,'mixtures':mixtures}
    (OUT/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'provenance':provenance,'basis_mapping':[
        {'bin':i,'harmonic':r['dominant_harmonic'],'amplitude':round(r['amplitude'],3),'phase':round(r['phase_radians'],6)} for i,r in enumerate(bases)],
        'amplitude_probes':[(r['input'][0],round(r['amplitude'],3)) for r in amplitudes],
        'output':str(OUT/'results.json')},indent=2))


if __name__ == '__main__':
    main()
