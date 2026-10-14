#!/usr/bin/env python3
"""Run OsTIrus' full TI synthesis engine against original and modified firmware."""
import hashlib
import json
from pathlib import Path
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'analysis/full-emulator'
RATE=48000


def schedule(table_id):
    events=[]
    def event(t, data):
        events.append((round(t*RATE),bytes(data)))
    event(0,(OUT/f'init-wavetable-id{table_id}.syx').read_bytes())
    event(.8,[0xb0,7,100])
    event(.8,[0xb0,11,127])
    segments=[]
    # Twelve stationary positions under one sustained note, after patch settling.
    event(1,[0x90,45,100])
    for i in range(12):
        start=1+i*.4
        position=round(i*127/11)
        event(start,[0xb0,17,position])
        segments.append({'kind':'position_hold','start':start,'end':start+.4,
                         'position':position,'note':45,'frame_index_nominal':i})
    event(5.8,[0x80,45,0])
    # Forward and reverse sweeps under a held note.
    event(6,[0x90,45,100])
    for i in range(128): event(6+i/127*2,[0xb0,17,i])
    for i in range(128): event(8+i/127*2,[0xb0,17,127-i])
    event(10.2,[0x80,45,0])
    segments.append({'kind':'sweep','start':6,'end':10.2,'note':45,'position_path':'0->127->0'})
    # Notes in three octaves, then a three-note chord.
    for i,note in enumerate([45,57,69]):
        start=10.5+i*.8
        event(start,[0xb0,17,64]);event(start,[0x90,note,100])
        event(start+.55,[0x80,note,0])
        segments.append({'kind':'note','start':start,'end':start+.55,'note':note,'position':64})
    for note in [48,52,55]:event(13,[0x90,note,100])
    for i in range(64):event(13+i/63,[0xb0,17,2*i])
    for note in [48,52,55]:event(14.2,[0x80,note,0])
    event(14.4,[0xb0,123,0])
    # Ask for the current single editbuffer; retained output verifies delivered state.
    event(14.6,[0xf0,0,0x20,0x33,1,0x10,0x30,0,0x40,0xf7])
    segments.append({'kind':'chord','start':13,'end':14.2,'notes':[48,52,55]})
    path=OUT/f'id{table_id}.events.txt'
    path.write_text(''.join(f'{sample} '+data.hex(' ')+'\n' for sample,data in sorted(events,key=lambda e:e[0])))
    return path,segments


def main():
    OUT.mkdir(exist_ok=True)
    source=json.loads((OUT/'container-manifest.json').read_text())
    binary=ROOT/'native/osti-render/build/osti-render'
    manifest={'engine_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
              'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'model':'TI: dual DSP, original DSP firmware, upstream modeled microcontroller',
              'sample_rate':RATE,'seconds':15,'container':source,'runs':[]}
    for table_id in [1,2]:
        events,segments=schedule(table_id)
        for label,rom in [('original',Path(source['original_bundle'])),
                          ('modified',OUT/'firmware.Prism-Formant.experimental.bin')]:
            name=f'{label}-id{table_id}'
            audio=OUT/f'{name}.f32'
            log=OUT/f'{name}.log'
            command=[str(binary),str(rom),str(events),str(audio),str(15*RATE)]
            print(f'START {name}',flush=True)
            begin=time.monotonic()
            with log.open('w') as f:
                run=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,timeout=180)
            if run.returncode:
                raise RuntimeError(f'{name} failed: exit {run.returncode}; see {log}')
            text=log.read_text()
            assert 'BOOT_COMPLETE' in text and 'DONE samples=720000' in text
            assert audio.stat().st_size==15*RATE*2*4
            record={'name':name,'rom_sha256':hashlib.sha256(rom.read_bytes()).hexdigest(),
                    'events_sha256':hashlib.sha256(events.read_bytes()).hexdigest(),
                    'audio_sha256':hashlib.sha256(audio.read_bytes()).hexdigest(),
                    'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest(),
                    'elapsed_seconds':round(time.monotonic()-begin,3),'segments':segments,
                    'completion':next(l for l in text.splitlines() if l.startswith('DONE'))}
            manifest['runs'].append(record)
            (OUT/'run-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
            print(f'DONE {name} seconds={record["elapsed_seconds"]} audio_sha256={record["audio_sha256"]}',flush=True)
    print('ALL_FOUR_RENDERS_COMPLETED',flush=True)


if __name__=='__main__':main()
