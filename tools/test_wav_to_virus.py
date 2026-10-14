#!/usr/bin/env python3
"""Focused format/math tests for the WAV importer (no hardware)."""
import math
from pathlib import Path
import struct
import tempfile
import unittest
from wav_to_virus import read_wav, analyze_cycle, convert, coefficient, interpolate_frames


def wav(path, values, kind=1, bits=16, channels=1):
    if kind==3:
        payload=b''.join(struct.pack('<f',v) for v in values)
    else:
        payload=b''.join(round(v*32767).to_bytes(2,'little',signed=True) for v in values)
    fmt=struct.pack('<HHIIHH',kind,channels,48000,48000*channels*bits//8,channels*bits//8,bits)
    chunks=b'fmt '+struct.pack('<I',len(fmt))+fmt+b'data'+struct.pack('<I',len(payload))+payload
    path.write_bytes(b'RIFF'+struct.pack('<I',len(chunks)+4)+b'WAVE'+chunks)


class ImportTests(unittest.TestCase):
    def test_known_spectrum_and_dc(self):
        x=[.1+.3*math.sin(2*math.pi*7*n/128+.7) for n in range(128)]
        a,m=analyze_cycle(x)
        self.assertAlmostEqual(a[6],.3)
        self.assertAlmostEqual(m['dc'],.1)
        self.assertLess(sum(a)-a[6],1e-13)

    def test_nyquist_scaling(self):
        a,_=analyze_cycle([.25*(-1)**n for n in range(64)])
        self.assertAlmostEqual(a[31],.25)

    def test_phase_is_discarded(self):
        a,_=analyze_cycle([.4*math.cos(2*math.pi*3*n/128) for n in range(128)])
        b,_=analyze_cycle([.4*math.cos(2*math.pi*3*n/128+1.23) for n in range(128)])
        self.assertLess(max(abs(x-y) for x,y in zip(a,b)),1e-14)

    def test_frame_axis_interpolation(self):
        out=interpolate_frames([[0]*32,[1]*32])
        self.assertEqual(len(out),12)
        for i,row in enumerate(out):
            self.assertAlmostEqual(row[0],i/11)

    def test_supported_containers(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'test.wav'
            for kind,bits in ((1,8),(1,16),(1,24),(1,32),(3,32),(3,64)):
                width=bits//8
                if kind==3:
                    payload=struct.pack('<f' if bits==32 else '<d',.25)*2
                elif bits==8:
                    payload=bytes([160])*2
                else:
                    payload=(1 << (bits-3)).to_bytes(width,'little',signed=True)*2
                fmt=struct.pack('<HHIIHH',kind,1,48000,48000*width,width,bits)
                for extensible in (False,True):
                    current=fmt
                    if extensible:
                        current=struct.pack('<H',0xfffe)+fmt[2:]+struct.pack('<HHI',22,bits,0)+struct.pack('<I',kind)+bytes.fromhex('00001000800000aa00389b71')
                    chunks=b'fmt '+struct.pack('<I',len(current))+current+b'data'+struct.pack('<I',len(payload))+payload
                    p.write_bytes(b'RIFF'+struct.pack('<I',len(chunks)+4)+b'WAVE'+chunks)
                    self.assertEqual(read_wav(p)[0],[.25,.25])

    def test_high_harmonic_energy(self):
        _,m=analyze_cycle([.3*math.sin(2*math.pi*40*n/128) for n in range(128)])
        self.assertAlmostEqual(m['above_32_energy'],.045)

    def test_reader_and_single_frame_replication(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'test.wav'
            wav(p,[.4*math.sin(2*math.pi*n/128) for n in range(128)])
            r=convert(p,128,Path(d)/'out')
            self.assertEqual(r['source_frames'],1)
            self.assertTrue(all(f==r['coefficients'][0] for f in r['coefficients']))
            self.assertTrue(all(q==0 for q in r['coefficients'][0][1:]))
            self.assertLessEqual(r['quantized_peak_bound'],.6)

    def test_float_stereo_mix_and_nonfinite_rejection(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'test.wav'
            wav(p,[.5,-.25]*64,kind=3,bits=32,channels=2)
            values,_=read_wav(p)
            self.assertEqual(values,[.125]*64)
            wav(p,[float('nan')],kind=3,bits=32)
            with self.assertRaisesRegex(ValueError,'Nonfinite'):
                read_wav(p)

    def test_silence_and_partial_cycle(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'test.wav'
            wav(p,[0]*64)
            r=convert(p,64,Path(d)/'out')
            self.assertEqual(sum(map(sum,r['coefficients'])),0)
            wav(p,[0]*65)
            with self.assertRaisesRegex(ValueError,'exact multiple'):
                convert(p,64,Path(d)/'out')

    def test_truncated_input(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'test.wav'
            wav(p,[0]*64)
            p.write_bytes(p.read_bytes()[:-1])
            with self.assertRaisesRegex(ValueError,'Truncated'):
                read_wav(p)

    def test_exact_gain_curve(self):
        self.assertEqual(coefficient(80),532744)
        self.assertEqual(coefficient(127),1342600)


if __name__=='__main__':
    unittest.main()
