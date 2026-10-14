#!/usr/bin/env python3
"""Create an original WAV, convert/patch/redecode it, and validate DSP replay."""
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import wave
from wav_to_virus import convert, model_cycle, coefficient, analyze_cycle, FULL
from replace_wavetable import build
from inspect_firmware import inspect
from map_dsp_load import map_image

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'analysis/wavetable-trace/wav-import/prism-formant'


def write_wav(path,samples):
    with wave.open(str(path),'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(3)
        w.setframerate(48000)
        w.writeframes(b''.join(round(v).to_bytes(3,'little',signed=True) for v in samples))


def preview(frames):
    size=len(frames[0])
    rate,duration=48000,6
    result=[]
    for n in range(rate*duration):
        t=n/(rate*duration-1)*(len(frames)-1)
        a=min(int(t),len(frames)-2)
        blend=t-a
        phase=(n*110/rate*size)%size
        s=int(phase)
        f=phase-s
        def sample(frame):
            return frame[s]*(1-f)+frame[(s+1)%size]*f
        v=sample(frames[a])*(1-blend)+sample(frames[a+1])*blend
        fade=min(1,n/(rate*.02),(rate*duration-1-n)/(rate*.02))
        result.append(v*fade)
    return result


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    source=OUT/'Prism-Formant-source-16x256.wav'
    # Original harmonic morph; no coefficients/samples borrowed from HarmncSweep.
    cycles=[]
    for frame in range(16):
        t=frame/15
        amps=[.18/(k**.8)+.32*math.exp(-.5*((k-(3+21*t))/(1.3+1.9*t))**2)
              +(.12/k**.5 if k%2 else 0) for k in range(1,41)]
        phases=[.29*k**1.37 for k in range(1,41)]
        cycles.append([.025+sum(a*math.cos(2*math.pi*k*n/256+p)
                               for k,(a,p) in enumerate(zip(amps,phases),1)) for n in range(256)])
    source_scale=.75*FULL/max(abs(v) for f in cycles for v in f)
    write_wav(source,[v*source_scale for f in cycles for v in f])
    converted=convert(source,256,OUT/'converted')
    image=OUT/'vti.Prism-Formant.experimental.bin'
    patch=build(ROOT/'analysis/firmware-images/vti.bin',1,OUT/'converted/coefficients.csv',image)
    inspect(image,OUT/'decoded')
    directory=OUT/'decoded'/image.stem
    map_image(directory)
    command=[sys.executable,str(ROOT/'tools/replay_wavetable.py'),
             '--load-dir',str(directory/'dsp-load'),
             '--config',str(ROOT/'analysis/dsp-probe-render-id1.txt'),
             '--image',str(image),'--output-dir',str(OUT/'replay')]
    run=subprocess.run(command,check=True,capture_output=True,text=True)
    (OUT/'replay-wrapper-output.txt').write_text(run.stdout)
    replay=json.loads((OUT/'replay/summary.json').read_text())
    rows=list(csv.reader((OUT/'replay/frames-12x64-signed24.csv').open()))[1:]
    actual=[[int(v) for v in row[1:]] for row in rows]
    prediction=[model_cycle(f) for f in converted['coefficients']]
    errors=[abs(a-b) for fa,fb in zip(actual,prediction) for a,b in zip(fa,fb)]
    assert max(errors)<64, ('Analytic model mismatch',max(errors))
    observed_spectra=[analyze_cycle([v/FULL for v in f])[0] for f in actual]
    magnitude_error=max(abs(a-b) for fa,fb in zip(observed_spectra,converted['quantized_amplitudes']) for a,b in zip(fa,fb))*FULL
    assert magnitude_error<8, ('Spectrum mismatch',magnitude_error)
    # Compare all band-limited levels against independently captured single-bin
    # responses. Fixed-point rounding accumulates, so this is tolerance-based.
    basis=json.loads((OUT.parent/'basis/results.json').read_text())['basis']
    raw=(OUT/'replay/rendered-table.24be.bin').read_bytes()
    words=[int.from_bytes(raw[i:i+3],'big',signed=True) for i in range(0,len(raw),3)]
    all_level_errors=[]
    for frame,qs in enumerate(converted['coefficients']):
        expected=[sum(basis[k]['all_levels'][n]*coefficient(q)/coefficient(80)
                      for k,q in enumerate(qs)) for n in range(294)]
        all_level_errors.extend(abs(a-b) for a,b in zip(words[45+frame*294:45+(frame+1)*294],expected))
    assert max(all_level_errors)<128, ('Band-limit basis mismatch',max(all_level_errors))
    assert max(abs(v) for f in actual for v in f)<FULL-1
    assert len({tuple(f) for f in converted['coefficients']})==12
    originals=json.loads((ROOT/'analysis/wavetable-trace/structures/wavetable-directory.json').read_text())['entries']
    assert converted['coefficient_payload_sha256'] not in {e['spectral_payload_sha256'] for e in originals}
    write_wav(OUT/'Prism-Formant-rendered-12x64.wav',[v for f in actual for v in f])
    write_wav(OUT/'Prism-Formant-preview.wav',preview(actual))
    with (OUT/'analytic-prediction.csv').open('w') as f:
        writer=csv.writer(f)
        writer.writerow(['frame']+[f'sample_{n}' for n in range(64)])
        writer.writerows([i]+row for i,row in enumerate(prediction))
    result={'status':'PASS: new WAV converted, full image rebuilt/decoded, original DSP builder executed',
            'source_wav_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'coefficient_payload_sha256':converted['coefficient_payload_sha256'],
            'replacement_sha256':patch['output_sha256'],'changed_firmware_bytes':patch['changed_bytes'],
            'bank_crcs':patch['bank_crcs'],'unique_new_frames':12,
            'spectral_relative_rms_quantization_error':converted['spectral_relative_rms_quantization_error'],
            'max_waveform_error_in_24bit_lsb':max(errors),
            'rms_waveform_error_in_24bit_lsb':math.sqrt(sum(v*v for v in errors)/len(errors)),
            'max_harmonic_amplitude_error_in_24bit_lsb':magnitude_error,
            'max_all_levels_basis_error_in_24bit_lsb':max(all_level_errors),
            'peak_absolute_sample':max(abs(v) for f in actual for v in f),
            'quantized_peak_bound_full_scale':converted['quantized_peak_bound'],
            'replay':replay,
            'scope':'Isolated original firmware generation code; allocation/timing stubs; not hardware or full oscillator playback',
            'provenance':{'converter_sha256':hashlib.sha256((ROOT/'tools/wav_to_virus.py').read_bytes()).hexdigest(),
                          'demo_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                          'harness_sha256':hashlib.sha256((ROOT/'native/dsp-probe/target/release/dsp-probe').read_bytes()).hexdigest()}}
    (OUT/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,separators=(',',':')))


if __name__=='__main__':
    main()
