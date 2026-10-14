#!/usr/bin/env python3
"""Measure recorded OsTIrus audio and export actual note/sweep renders."""
import hashlib
import json
import math
from pathlib import Path
import wave
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'analysis/full-emulator'
RATE=48000


def frequency(signal,start,end,expected):
    y=signal[round(start*RATE):round(end*RATE)]
    size=262144
    spectrum=np.abs(np.fft.rfft(y*np.hanning(len(y)),n=size))
    hz=np.fft.rfftfreq(size,1/RATE)
    candidates=np.where((hz>expected*.97)&(hz<expected*1.03))[0]
    k=candidates[np.argmax(spectrum[candidates])]
    a,b,c=np.log(np.maximum(spectrum[k-1:k+2],1e-30))
    offset=.5*(a-c)/(a-2*b+c)
    return float((k+offset)*RATE/size)


def harmonics(signal,start,end,fundamental):
    indices=np.arange(round(start*RATE),round(end*RATE),4)
    angles=2*np.pi*indices[:,None]/RATE*fundamental*np.arange(1,33)[None,:]
    design=np.c_[np.ones(len(indices)),np.cos(angles),np.sin(angles)]
    coefficients=np.linalg.lstsq(design,signal[indices],rcond=None)[0]
    return np.hypot(coefficients[1:33],coefficients[33:65])


def write_wav(path,signal):
    if np.max(np.abs(signal))>=1:raise ValueError('Cannot export clipped data')
    pcm=np.rint(signal*(2**23)).astype('<i4').view('u1').reshape(-1,4)[:,:3].tobytes()
    with wave.open(str(path),'wb') as w:
        w.setnchannels(2);w.setsampwidth(3);w.setframerate(RATE);w.writeframes(pcm)


def main():
    manifest=json.loads((OUT/'run-manifest.json').read_text())
    audio={}
    stats={}
    for run in manifest['runs']:
        name=run['name'];data=(OUT/(name+'.f32')).read_bytes()
        assert hashlib.sha256(data).hexdigest()==run['audio_sha256']
        x=np.frombuffer(data,dtype='<f4').reshape(-1,2).astype(float)
        assert x.shape==(720000,2) and np.isfinite(x).all()
        assert np.max(abs(x))<1
        assert np.max(abs(x[-9600:]))==0
        audio[name]=x
        stats[name]={'peak':float(np.max(abs(x))),'rms':float(np.sqrt(np.mean(x*x))),
                     'final_200ms_silent':True,'finite':True,'unclipped':True}
        # Read back the current patch through the engine's MIDI response.
        dumps=[]
        for line in (OUT/(name+'.f32.midi-out.txt')).read_text().splitlines():
            fields=line.split()
            if len(fields)!=525:continue
            message=bytes(int(v,16) for v in fields[1:])
            assert message[265]==sum(message[5:265])%128
            assert message[522]==sum(message[5:522])%128
            patch=message[9:265]+message[266:522]
            dumps.append({'sample':int(fields[0]),'name':patch[240:250].decode(),
                          'oscillator_mode':patch[286],'wavetable_id':patch[19],
                          'position':patch[17],'oscillator_balance':patch[33]})
        assert len(dumps)==1 and dumps[0]['oscillator_mode']==2
        assert dumps[0]['wavetable_id']==int(name[-1]) and dumps[0]['position']==126
        stats[name]['patch_readback']=dumps[0]
        write_wav(OUT/(name+'.wav'),x)
    assert np.array_equal(audio['original-id2'],audio['modified-id2'])
    assert not np.array_equal(audio['original-id1'],audio['modified-id1'])
    x=audio['modified-id1'][:,0]
    notes=[]
    for i,note in enumerate([45,57,69]):
        expected=440*2**((note-69)/12)
        measured=frequency(x,10.65+i*.8,10.95+i*.8,expected)
        cents=1200*math.log2(measured/expected)
        assert abs(cents)<5
        notes.append({'note':note,'expected_hz':expected,'measured_hz':measured,'error_cents':cents})
    # Expected spectra come from the newly encoded bytes, not the rendered audio.
    expected=np.array(json.loads((ROOT/'analysis/wavetable-trace/wav-import/prism-formant/converted/conversion.json').read_text())['quantized_amplitudes'])
    grid=np.linspace(0,11,1101)
    targets=[]
    for position in grid:
        lo=int(position);hi=min(lo+1,11);f=position-lo
        targets.append(expected[lo]*(1-f)+expected[hi]*f)
    targets=np.array(targets);targets/=np.linalg.norm(targets,axis=1)[:,None]
    def fit(amplitudes):
        similarity=targets@(amplitudes/np.linalg.norm(amplitudes))
        i=np.argmax(similarity)
        return float(grid[i]),float(similarity[i])
    holds=[]
    for i in range(12):
        start=1+i*.4+.18;end=1+i*.4+.38
        hz=frequency(x,start,end,110)
        amps=harmonics(x,start,end,hz)
        position,similarity=fit(amps)
        holds.append({'cc17':round(i*127/11),'inferred_frame_position':position,
                      'spectral_cosine_similarity':similarity,'fundamental_hz':hz,
                      'harmonic_amplitudes':amps.tolist(),
                      'harmonic_centroid':float(amps@np.arange(1,33)/sum(amps))})
    assert all(b['inferred_frame_position']>a['inferred_frame_position'] for a,b in zip(holds,holds[1:]))
    assert min(h['spectral_cosine_similarity'] for h in holds)>.90
    assert holds[0]['inferred_frame_position']<.5 and holds[-1]['inferred_frame_position']>10
    sweeps=[]
    for direction,start in [('forward',6),('reverse',8)]:
        rows=[]
        for center in np.linspace(start+.15,start+1.85,18):
            hz=frequency(x,center-.075,center+.075,110)
            amps=harmonics(x,center-.075,center+.075,hz)
            position,similarity=fit(amps)
            rows.append({'seconds':float(center),'inferred_frame_position':position,'similarity':similarity})
        positions=np.array([r['inferred_frame_position'] for r in rows])
        correlation=float(np.corrcoef(np.arange(len(rows)),positions)[0,1])
        assert correlation>.98 if direction=='forward' else correlation<-.98
        sweeps.append({'direction':direction,'time_position_correlation':correlation,'windows':rows})
    # Check actual voiced energy at all chord fundamentals simultaneously.
    chord=[]
    for note in [48,52,55]:
        expected_hz=440*2**((note-69)/12)
        measured=frequency(x,13.25,13.65,expected_hz)
        amp=harmonics(x,13.25,13.65,measured)[0]
        assert amp>1e-4
        chord.append({'note':note,'measured_hz':measured,'fundamental_amplitude':float(amp)})
    clip=audio['modified-id1'][RATE:round(14.5*RATE)]
    write_wav(OUT/'Prism-Formant-full-engine-demo.wav',clip)
    original=audio['original-id1'][6*RATE:round(10.2*RATE)]
    modified=audio['modified-id1'][6*RATE:round(10.2*RATE)]
    # Audition-only edge fades; full .wav files retain the raw output unchanged.
    a=original.copy();b=modified.copy();fade=np.linspace(0,1,480)
    for v in [a,b]:v[:480]*=fade[:,None];v[-480:]*=fade[::-1,None]
    write_wav(OUT/'HarmncSweep-then-Prism-Formant-AB.wav',np.concatenate([a,np.zeros((19200,2)),b]))
    result={'status':'PASS','sample_rate':RATE,'stats':stats,'notes':notes,'position_holds':holds,
            'sweeps':sweeps,'chord':chord,'untouched_id2_bit_identical':True,
            'modified_id1_differs':True,'peak_full_engine_demo':float(np.max(abs(clip))),
            'position_model_scope':'Best fit of harmonic magnitudes against linearly blended source spectra, allowing overall gain; complete voice/filter/interpolation response means this is not a bit-exact waveform comparison',
            'analysis_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (OUT/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'status':'PASS','untouched_id2_bit_identical':True,'notes':notes,
                      'minimum_hold_similarity':min(h['spectral_cosine_similarity'] for h in holds),
                      'sweep_correlations':[s['time_position_correlation'] for s in sweeps],
                      'peak':result['peak_full_engine_demo']}))


if __name__=='__main__':main()
