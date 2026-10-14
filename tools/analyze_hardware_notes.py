#!/usr/bin/env python3
"""Offline verification of the private-MIDI hardware bench note capture."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import wave

import numpy as np


def read_capture(path):
    data = path.read_bytes()
    assert data[:4] == b'RIFF' and data[8:16] == b'WAVEfmt '
    assert struct.unpack_from('<HHI', data, 20) == (3, 6, 48000)
    assert data[36:40] == b'data'
    assert struct.unpack_from('<I', data, 40)[0] == len(data) - 44
    return np.frombuffer(data[44:], '<f4').reshape(-1, 6)


def pitch(signal, start, end, expected):
    segment = signal[round(start * 48000):round(end * 48000)]
    size = 262144
    spectrum = abs(np.fft.rfft(segment * np.hanning(len(segment)), n=size))
    hz = np.fft.rfftfreq(size, 1 / 48000)
    candidates = np.where((hz > expected * .97) & (hz < expected * 1.03))[0]
    k = candidates[np.argmax(spectrum[candidates])]
    a, b, c = np.log(np.maximum(spectrum[k-1:k+2], 1e-30))
    return float((k + .5 * (a-c) / (a-2*b+c)) * 48000 / size)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    root = args.directory
    result = json.loads((root / 'result.json').read_text())
    stats = result['stream_test']
    assert stats['frames'] == stats['bridge_rendered'] == 384000
    for field in ['callback_errors', 'timestamp_gaps', 'publish_failures',
                  'bridge_underruns', 'bridge_midi_dropped']:
        assert stats[field] == 0, field
    assert stats['bridge_midi_queued'] == 6
    raw = read_capture(root / 'raw-usb.wav')
    rendered = read_capture(root / 'vst3-returns.wav')
    assert raw.shape == rendered.shape == (384000, 6)
    assert np.isfinite(raw).all() and np.isfinite(rendered).all()
    mapping = [i-1 for i in stats['plugin_channel_to_raw_channel']]
    assert mapping == [2, 3, 4, 5, 0, 1]
    assert np.array_equal(rendered, raw[:, mapping])
    assert result['test_patch_exact'] and result['routing_readback'] == 10
    restored = {}
    for name in ['part-0', 'part-64', 'multi']:
        before = (root / ('before-' + name + '.syx')).read_bytes()
        after = (root / ('restored-' + name + '.syx')).read_bytes()
        assert before == after, name
        for message in [before, after]:
            assert message[265] == sum(message[5:265]) % 128
            if len(message) == 524:
                assert message[522] == sum(message[5:522]) % 128
        restored[name] = True
    wire = (root / 'private-out.bin').read_bytes()
    packets = [wire[i:i+4] for i in range(0, len(wire), 4)]
    assert len(wire) % 4 == 0
    note_packets = [p.hex() for p in packets if p[0] in (0x28, 0x29)]
    expected_packets = [f'{prefix}{status}{key:02x}{velocity:02x}'
                        for key in [60, 64, 68]
                        for prefix, status, velocity in [('29', '90', 36), ('28', '80', 0)]]
    assert note_packets[:6] == expected_packets
    # Cleanup sends the same three explicit note-offs once more.
    assert note_packets[6:] == [f'2880{key:02x}00' for key in [60, 64, 68]]
    signal = rendered[:, 0].astype(float)
    notes = []
    for index, key in enumerate([60, 64, 68]):
        start = 1 + index * 2
        expected = 440 * 2 ** ((key - 69) / 12)
        measured = pitch(signal, start + .2, start + .8, expected)
        cents = 1200 * math.log2(measured / expected)
        segment = signal[round((start+.2)*48000):round((start+.8)*48000)]
        rms = float(np.sqrt(np.mean(segment ** 2)))
        assert abs(cents) < 5 and rms > .005
        notes.append(dict(midi_note=key, expected_hz=expected,
                          measured_hz=measured, error_cents=cents, rms=rms))
    assert np.max(abs(signal[:43200])) == 0
    assert np.max(abs(signal[round(6.3*48000):])) == 0
    for a, b in [(2.3, 2.9), (4.3, 4.9)]:
        assert np.max(abs(signal[round(a*48000):round(b*48000)])) <= 2/32768
    stereo = rendered[:, :2]
    assert np.max(abs(stereo)) < 1
    with wave.open(str(root / 'hardware-three-notes.wav'), 'wb') as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(48000)
        output.writeframes(np.rint(stereo * 32768).astype('<i2').tobytes())
    report = dict(status='PASS', notes=notes, restoration_byte_exact=restored,
                  raw_to_plugin_bit_exact=True, plugin_channel_to_raw_channel=[3,4,5,6,1,2],
                  peak=float(np.max(abs(stereo))), final_note_off_silent=True,
                  note_packets=note_packets,
                  scope='Actual development VST3 in custom bench host; stock hardware firmware; immediate block-level MIDI, not sample-accurate scheduling',
                  hashes={name: hashlib.sha256((root/name).read_bytes()).hexdigest()
                          for name in ['raw-usb.wav', 'vst3-returns.wav', 'private-out.bin', 'hardware-three-notes.wav']})
    (root / 'note-validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
