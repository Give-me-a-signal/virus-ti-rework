#!/usr/bin/env python3
"""Convert a cycle-aligned WAV wavetable to the verified HarmncSweep spectral format.

Standard-library only. Input cycles must be explicit; this is not pitch detection
or a general recording-to-wavetable synthesizer. Output preserves magnitudes,
discards DC/phase/harmonics above 32, and fits 12 frames with the original gain.
"""
import argparse
import cmath
import csv
import hashlib
import json
import math
from pathlib import Path
import struct

FULL = 1 << 23
GAIN = 0x14CF73


def coefficient(q):
    return ((q*q << 9) * GAIN) >> 23


def read_wav(path):
    data = path.read_bytes()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise ValueError('Expected little-endian RIFF/WAVE')
    end = 8 + int.from_bytes(data[4:8], 'little')
    if end > len(data):
        raise ValueError('Truncated RIFF')
    fmt = payload = None
    pos = 12
    while pos < end:
        if pos+8 > end:
            raise ValueError('Truncated chunk header')
        tag, size = data[pos:pos+4], int.from_bytes(data[pos+4:pos+8], 'little')
        start = pos+8
        if start+size > end:
            raise ValueError('Truncated WAV chunk')
        if tag == b'fmt ':
            if fmt is not None:
                raise ValueError('Multiple format chunks')
            fmt = data[start:start+size]
        if tag == b'data':
            if payload is not None:
                raise ValueError('Multiple data chunks are unsupported')
            payload = data[start:start+size]
        pos = start+size+(size & 1)
    if fmt is None or len(fmt)<16 or payload is None:
        raise ValueError('Missing WAV format or sample data')
    kind, channels, rate, byte_rate, align, bits = struct.unpack_from('<HHIIHH', fmt)
    if kind == 0xfffe:
        if len(fmt)<40 or struct.unpack_from('<H',fmt,16)[0]<22:
            raise ValueError('Invalid extensible WAV format')
        valid_bits = struct.unpack_from('<H',fmt,18)[0]
        if valid_bits not in (0,bits):
            raise ValueError('Packed valid-bit/container-bit mismatch is unsupported')
        if fmt[28:40] != bytes.fromhex('00001000800000aa00389b71'):
            raise ValueError('Unsupported WAV subtype GUID')
        kind = int.from_bytes(fmt[24:28],'little')
    if channels<1 or channels>32 or rate<1:
        raise ValueError('Invalid channel count or sample rate')
    if (kind == 1 and bits not in (8,16,24,32)) or (kind == 3 and bits not in (32,64)) or kind not in (1,3):
        raise ValueError('Supported formats: PCM8/16/24/32, float32/64')
    width = bits//8
    if align != channels*width or byte_rate != rate*align or len(payload)%align:
        raise ValueError('WAV block alignment/length mismatch')
    values = []
    for start in range(0,len(payload),align):
        frame = []
        for channel in range(channels):
            raw = payload[start+channel*width:start+(channel+1)*width]
            if kind==3:
                v = struct.unpack('<f' if bits==32 else '<d',raw)[0]
            elif bits==8:
                v = (raw[0]-128)/128
            else:
                v = int.from_bytes(raw,'little',signed=True)/(1 << (bits-1))
            if not math.isfinite(v):
                raise ValueError('Nonfinite WAV sample')
            frame.append(v)
        values.append(sum(frame)/channels)
    return values, {'format':kind,'bits':bits,'channels':channels,'sample_rate':rate,
                    'mono_sample_count':len(values),'channel_policy':'arithmetic mean',
                    'sha256':hashlib.sha256(data).hexdigest()}


def analyze_cycle(samples):
    n = len(samples)
    # No window: each block is explicitly one complete periodic cycle.
    spectrum = [sum(v*cmath.exp(-2j*math.pi*k*i/n) for i,v in enumerate(samples))/n
                for k in range(33)]
    magnitudes = [abs(spectrum[k])*(1 if 2*k==n else 2) for k in range(1,33)]
    represented_energy = sum(a*a*(1 if 2*k==n else .5) for k,a in enumerate(magnitudes,1))
    ac_energy = sum((v-spectrum[0].real)**2 for v in samples)/n
    return magnitudes, {'dc':spectrum[0].real,'ac_energy':ac_energy,
                        'above_32_energy':max(0,ac_energy-represented_energy)}


def interpolate_frames(frames):
    out=[]
    for i in range(12):
        position = i*(len(frames)-1)/11
        lo = int(position)
        hi = min(lo+1,len(frames)-1)
        fraction = position-lo
        out.append([a*(1-fraction)+b*fraction for a,b in zip(frames[lo],frames[hi])])
    return out


def model_cycle(coefficients):
    # Original cache order: half-cycle rotated, with half-sample sine phase.
    c = [coefficient(q) for q in coefficients]
    return [sum(c[k-1]*math.cos(2*math.pi*k*n/64-math.pi/2+k*math.pi+k*math.pi/64)
                for k in range(1,32)) + c[31]*.5*(-1)**n for n in range(64)]


def convert(source, cycle_samples, output, headroom=.6):
    if not 64 <= cycle_samples <= 65536:
        raise ValueError('Cycle size must be 64..65536 samples')
    if not 0 < headroom <= .8:
        raise ValueError('Headroom must be >0 and <=0.8 full scale')
    samples, metadata = read_wav(source)
    if not samples or len(samples)%cycle_samples:
        raise ValueError('WAV length must be a positive exact multiple of --cycle-samples')
    if len(samples)//cycle_samples > 4096:
        raise ValueError('At most 4096 source cycles are supported')
    analyzed = [analyze_cycle(samples[i:i+cycle_samples]) for i in range(0,len(samples),cycle_samples)]
    amplitudes = interpolate_frames([a for a,_ in analyzed])
    # Last firmware bin produces half its coefficient amplitude at Nyquist.
    weights = [[a*(2 if k==31 else 1) for k,a in enumerate(f)] for f in amplitudes]
    largest = max(max(f) for f in weights)
    max_sum = max(sum(f) for f in amplitudes)
    max_coefficient = coefficient(127)/FULL
    scale = min(.95*max_coefficient/largest, headroom/max_sum) if largest>1e-14 else 1.0
    levels = [coefficient(q)/FULL for q in range(128)]
    encoded = [[min(range(128), key=lambda q:abs(levels[q]-a*scale)) for a in f] for f in weights]
    # Verify the quantized harmonic sum, not merely the unquantized request.
    def bound(frame):
        return sum(coefficient(q)*(0.5 if k==31 else 1) for k,q in enumerate(frame))/FULL
    while max(map(bound,encoded)) > headroom + 1e-12:
        scale *= .99
        encoded = [[min(range(128),key=lambda q:abs(levels[q]-a*scale)) for a in f] for f in weights]
    output.mkdir(parents=True,exist_ok=True)
    with (output/'coefficients.csv').open('w') as file:
        writer=csv.writer(file)
        writer.writerow(['frame']+[f'bin_{k}' for k in range(32)])
        writer.writerows([i]+frame for i,frame in enumerate(encoded))
    target = [[a*scale for a in f] for f in amplitudes]
    reconstructed = [[coefficient(q)/FULL*(.5 if k==31 else 1) for k,q in enumerate(f)] for f in encoded]
    error = sum((a-b)**2 for fa,fb in zip(target,reconstructed) for a,b in zip(fa,fb))
    energy = sum(a*a for f in target for a in f)
    report={'source':str(source.resolve()),'source_wav':metadata,'cycle_samples':cycle_samples,
            'source_frames':len(analyzed),'output_frames':12,'output_harmonics':32,
            'frame_mapping':'Linear interpolation of harmonic magnitudes across source cycles; one cycle repeats',
            'phase_policy':'Discard input phase and use original ID1 flag0 phase; not arbitrary PCM reconstruction',
            'gain_header':'0x5B7A88','decoded_gain':'0x14CF73','normalization_scale':scale,
            'headroom_full_scale':headroom,'quantized_peak_bound':max(map(bound,encoded)),
            'spectral_relative_rms_quantization_error':math.sqrt(error/energy) if energy else 0,
            'source_analysis':[m for _,m in analyzed],
            'target_amplitudes':target,'quantized_amplitudes':reconstructed,
            'coefficients':encoded,'coefficient_payload_sha256':hashlib.sha256(bytes(q for f in encoded for q in f)).hexdigest()}
    (output/'conversion.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('--cycle-samples',type=int,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--headroom',type=float,default=.6)
    args=parser.parse_args()
    r=convert(args.source,args.cycle_samples,args.output,args.headroom)
    print(json.dumps({k:r[k] for k in ('source','source_frames','output_frames','normalization_scale','quantized_peak_bound','spectral_relative_rms_quantization_error','coefficient_payload_sha256')},indent=2))
