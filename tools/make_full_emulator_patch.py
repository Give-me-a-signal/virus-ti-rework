#!/usr/bin/env python3
"""Prepare an isolated oscillator test patch from this installer's factory INIT."""
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'analysis/full-emulator'
SOURCE = ROOT / 'analysis/firmware-images/decoded/vti/P065.bin'
XML = ROOT / 'analysis/package-expanded/Core_components.pkg/Payload/Library/Application Support/Access Music/Virus TI/Common/Virus Control.bundle/Contents/Resources/remotedevice.xml'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def sysex(patch):
    # Matches Microcontroller::createSingleDump and calcChecksum(offset=5).
    msg = bytearray([0xf0, 0, 0x20, 0x33, 1, 0x10, 0x10, 0, 0x40])
    msg.extend(patch[:256])
    msg.append(sum(msg[5:]) & 127)
    msg.extend(patch[256:])
    msg.append(sum(msg[5:]) & 127)
    msg.append(0xf7)
    assert len(msg) == 524
    assert max(msg[1:-1]) < 128
    return msg

def main():
    DEST.mkdir(parents=True, exist_ok=True)
    original = SOURCE.read_bytes()[0x7e00:0x8000]
    assert original[240:250] == b' -INIT-   '
    assert len(original) == 512 and original[0] == 8
    root = ET.parse(XML).getroot()
    bases = {'cc': 0, 'pp': 128, 'Bank C (6E) Single-3': 256,
             'Bank D (6F) Single-4': 384}
    params = {}
    for group in root.findall('.//ParameterSet'):
        for p in group.findall('Parameter'):
            base = bases.get(p.get('defaultmessage'))
            if base is not None:
                params[group.get('name')+'/'+p.get('name')] = base + int(p.get('controllerindex'))
    patch = bytearray(original)
    edits = []
    def setv(name, value):
        offset = params[name]
        edits.append({'parameter': name, 'offset': offset, 'original': patch[offset], 'value': value})
        patch[offset] = value
    values = {
        'Oscillator 1/Mode': 2,
        'Oscillator 1 Wavetable/WaveTableIndex': 0,
        'Oscillator 1 Wavetable/WaveTableSelect': 1,
        'Oscillator 1/Balance': 0,
        'Oscillators/Phase Init': 64,
        'Oscillators/Volume+Saturation': 64,
        'Oscillator 3/Mode': 0,
        'Suboscillator/Volume': 0,
        'Noise Oscillator/Volume': 0,
        'Ring Modulator/Volume': 0,
        'Oscillator 2/FM Amount': 0,
        'Unison/Mode': 0,
        'Filters/Cutoff Link': 0,
        'Filter 1/Cutoff': 127,
        'Filter 2/Cutoff': 127,
        'Filter 1/Resonance': 0,
        'Filter 2/Resonance': 0,
        'Filter 1/Envelope Amount': 0,
        'Filter 2/Envelope Amount': 0,
        'Voice Saturation/Type': 0,
        'Amp Envelope/Attack': 0,
        'Amp Envelope/Decay': 127,
        'Amp Envelope/Sustain': 127,
        'Amp Envelope/Sustain Time': 64,
        'Amp Envelope/Release': 4,
        'Patch Common/Volume': 100,
        'Patch Common/Panorama': 64,
        'Patch Common/Punch Intensity': 0,
        'Arpeggiator/Mode': 0,
        'Delay/Send': 0,
        'Reverb/Send': 0,
        'Chorus/Mix': 0,
        'Chorus/Mix2': 0,
        'Phaser/Mix': 0,
        'Patch Distortion/Type': 0,
        'Filter Bank/Type': 0,
        'Filter Bank/Mix': 0,
        'Input/Mode': 0,
        'Input Ring Modulator/Mix': 0,
        'EQ/Low Gain': 64,
        'EQ/Mid Gain': 64,
        'EQ/High Gain': 64,
    }
    for name, value in values.items():
        setv(name, value)
    # Factory INIT has an active velocity->volume modulation slot. Explicitly
    # disable all six slots, including TI's added second/third destinations.
    for name in params:
        if name.startswith('Assign Slot '):
            setv(name, 64 if '/Amount ' in name else 0)
        if name.startswith(('LFO 1/', 'LFO 2/')) and '/Dest ' in name:
            setv(name, 0 if name.endswith('Target') else 64)
        if name.startswith('LFO 3/') and '/Dest ' in name:
            setv(name, 0)
        if name.startswith('Velocity Response/'):
            setv(name, 64)
    out = {'source': str(SOURCE.relative_to(ROOT)), 'source_sha256': digest(SOURCE.read_bytes()),
           'source_offset': '0x7e00', 'source_size': 512,
           'original_name': original[240:250].decode(), 'original_patch_sha256': digest(original),
           'xml_sha256': digest(XML.read_bytes()), 'edits': edits, 'outputs': {}}
    (DEST/'factory-init.bin').write_bytes(original)
    (DEST/'factory-init.syx').write_bytes(sysex(original))
    for table_id in [1, 2]:
        patch[19] = table_id
        patch[240:250] = f'WT TEST {table_id:02d}'.encode()
        for ext, data in [('bin', patch), ('syx', sysex(patch))]:
            path = DEST/f'init-wavetable-id{table_id}.{ext}'
            path.write_bytes(data)
            out['outputs'][path.name] = {'size': len(data), 'sha256': digest(data)}
    (DEST/'init-patch-provenance.json').write_text(json.dumps(out, indent=2)+'\n')
    print(json.dumps(out['outputs'], indent=2))

if __name__ == '__main__':
    main()
