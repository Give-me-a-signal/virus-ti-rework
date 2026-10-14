# Importing a new WAV into the HarmncSweep slot

**A completely new WAV-derived table now works in the isolated DSP emulator.** The new “Prism Formant” table replaces all 384 HarmncSweep coefficient bytes. The original firmware routines generate all 12 frames and six band-limited levels, and their output agrees with the recovered spectral model. No device was flashed.

The [subsequent full OsTIrus test](full-emulator-test.md) also passes: the new table plays through complete voices with MIDI notes, chords and position sweeps. Physical hardware remains untested.

The importer preserves harmonic magnitudes within this slot's limits. It does **not** preserve arbitrary WAV phase: the firmware imposes its own phase relationships. This is a constrained spectral resynthesis of a cycle-aligned WAV, not lossless PCM storage.

## Deliverables

- [WAV converter](../tools/wav_to_virus.py), standard-library Python with no extra dependencies.
- [Original test WAV: Prism Formant, 16 cycles × 256 samples](../analysis/wavetable-trace/wav-import/prism-formant/Prism-Formant-source-16x256.wav).
- [Converted coefficient CSV](../analysis/wavetable-trace/wav-import/prism-formant/converted/coefficients.csv) and [conversion measurements](../analysis/wavetable-trace/wav-import/prism-formant/converted/conversion.json).
- [Firmware-rendered wavetable: 12 × 64 samples](../analysis/wavetable-trace/wav-import/prism-formant/Prism-Formant-rendered-12x64.wav).
- [Six-second listening preview](../analysis/wavetable-trace/wav-import/prism-formant/Prism-Formant-preview.wav).
- [Experimental replacement image](../analysis/wavetable-trace/wav-import/prism-formant/vti.Prism-Formant.experimental.bin) and [patch manifest](../analysis/wavetable-trace/wav-import/prism-formant/vti.Prism-Formant.experimental.bin.json).
- [Emulator validation](../analysis/wavetable-trace/wav-import/prism-formant/validation.json), [complete generated cache](../analysis/wavetable-trace/wav-import/prism-formant/replay/rendered-table.24be.bin), and [full replay output](../analysis/wavetable-trace/wav-import/prism-formant/replay/replay-output.txt).

The preview uses a 110 Hz oscillator with linear cycle/frame interpolation and short fades. It auditions the recovered cycles; it does not emulate the complete Virus oscillator, filters, effects or voice engine. The firmware slot and its displayed name remain ID 1 / HarmncSweep.

## Recovered representation

The unchanged ID 1 header is `[5,12,0,0x5B7A88]`: 32 coefficients per frame, 12 frames, original flag and gain. The [earlier trace](wavetable-replacement.md) maps it to F029 and records its encoded-file offsets.

For each byte `q` in 0–127, the unpacker produces:

```text
C(q) = floor(q² × 0x14CF73 / 16384)
C(80) = 532744
C(127) = 1342600
```

The first 31 bytes control harmonics 1–31. The final byte controls harmonic 32, the Nyquist component of the 64-sample cycle, at **half** the coefficient amplitude. There is no stored DC term. In the original cache order, the unsaturated mathematical model is:

```text
y[n] = Σ(k=1..31) C(q[k−1]) sin(2πk(n+0.5)/64 + πk)
       + C(q[31])/2 × (−1)^n
```

Firmware fixed-point rounding causes small deviations. The sine phase includes the half-sample offset and the cache's half-cycle rotation. It cannot encode arbitrary independent harmonic phases or negative amplitudes.

Static trace: `0x5FCB7` clears DC; `0x5FCB8..0x5FCC0` constructs complex harmonics; `0x5FCC1..0x5FCC4` writes half the last coefficient at Nyquist; `0x5FCCC` invokes the transform; `0x5FCD7..0x5FCE6` rotates/copies output. [Focused instruction notes](../analysis/wavetable-trace/wav-import/static-notes.md).

The [basis experiment](../tools/characterize_wavetable.py) executes 32 single-bin probes, 12 amplitude probes, and 12 mixed/silent probes against the original instructions. [Recorded responses](../analysis/wavetable-trace/wav-import/basis/results.json). It confirms the harmonic/phase mapping and squared amplitude law. Dense, high-amplitude test spectra saturate, so the importer must limit the harmonic sum, not just each coefficient.

## Conversion procedure

1. Read PCM8/16/24/32 or float32/64 WAV, including supported WAVE_FORMAT_EXTENSIBLE variants. Multiple channels are averaged. Reject malformed/truncated input and nonfinite samples.
2. Split at the explicitly supplied cycle length. Every block must be one full periodic cycle; no pitch detection or automatic cycle finding is performed. A single cycle is repeated across the output table. Ordinary recordings require cycle selection before import.
3. Compute Fourier magnitudes for harmonics 1–32 from each source cycle, without a window. Record and discard DC and energy above harmonic 32. Preserve correct Nyquist scaling when the input cycle itself has 64 samples.
4. Interpolate the sequence of magnitude vectors to 12 frames. This preserves harmonic morphs; it is not interpolation of arbitrary input phases.
5. Apply one common amplitude scale to the entire table, bounded by coefficient capacity and a conservative sum-of-harmonics peak bound. The default bound is 0.6 full scale; quantized results are checked against it too. This retains frame-to-frame level relationships.
6. For each desired amplitude, choose the nearest value from the exact `C(q)` lookup. For harmonic 32, request twice the desired coefficient amplitude to compensate for the firmware's half-amplitude rule. Pack the resulting 12 × 32 byte values through the existing image replacement tool and rebuild F029's CRC.

The operation resembles square-root quantization, but the converter uses the exact integer lookup rather than an approximate inverse formula. The original gain, flag, frame count and directory remain unchanged.

## Validation of the new table

The test input is an original 16-frame, 256-sample harmonic morph generated from analytic sinusoids with a moving spectral peak and varying harmonic weights. It also has nontrivial phase, DC and some harmonics 33–40 to exercise the format's losses. No HarmncSweep samples or spectral values are used to design it.

The converter resamples its frame sequence to 12 unique frames. Its coefficient payload differs from every original stored table. The full patched image is decoded again before replay, ensuring the test covers packing and source mapping as well as generation.

| Check | Result |
|---|---:|
| Source frames → destination frames | 16 → 12 |
| Changed image bytes | 384 coefficients + 2 CRC bytes |
| F029 CRC | `0x197B` → `0x9CBC` |
| Firmware builder execution | 122,319 instructions; normal completion |
| Generated levels | All 12 × 6 |
| Cycle guards | All 72 match |
| Clipping | None; first-level peak ≈ 0.4323 full scale |
| Byte-quantization spectral RMS error | 1.039% relative to scaled target magnitudes |
| Maximum harmonic-amplitude error vs quantized target | 1.50 signed-24-bit sample units |
| Waveform error vs analytic model | 3.40 RMS / 18.31 maximum sample units |
| All-level error vs superposed single-bin responses | 13.62 maximum sample units |

The 1.039% figure measures magnitude quantization only. It excludes discarded phase, DC and frequencies above 32, and does not claim waveform identity to the source WAV. In this test, discarded harmonics above 32 account for approximately 0.552% of source AC energy. Common amplitude scaling is approximately 0.54535.

The [11 focused tests](../tools/test_wav_to_virus.py) pass: known Fourier amplitudes/DC, Nyquist, phase removal, frame interpolation, supported integer/float/extensible containers, channel averaging, silence, incomplete cycles, malformed files, nonfinite samples and exact gain arithmetic. The full demo additionally checks new-frame uniqueness, clipping, all levels and guard samples.

Image SHA-256: `e219474ac54c00a1be9c982f5649929991e1d3932b490d4584720636467d2cbc`.

## Use with another WAV

Run from the workspace root. These examples assume a WAV consisting of concatenated **2048-sample cycles**; change that value to the actual cycle length. The sample-rate tag does not establish the cycle length. Omit `rtk proxy` if RTK is unavailable.

```sh
rtk proxy python3 tools/wav_to_virus.py /absolute/path/to/table.wav \
  --cycle-samples 2048 --output analysis/my-wav-table

rtk proxy python3 tools/replace_wavetable.py --id 1 \
  --coefficients analysis/my-wav-table/coefficients.csv \
  --output analysis/my-wav-table/vti.custom.experimental.bin

rtk proxy python3 tools/inspect_firmware.py \
  analysis/my-wav-table/vti.custom.experimental.bin \
  --output analysis/my-wav-table/decoded

rtk proxy python3 tools/map_dsp_load.py \
  analysis/my-wav-table/decoded/vti.custom.experimental

rtk proxy python3 tools/replay_wavetable.py \
  --load-dir analysis/my-wav-table/decoded/vti.custom.experimental/dsp-load \
  --config analysis/dsp-probe-render-id1.txt \
  --image analysis/my-wav-table/vti.custom.experimental.bin \
  --output-dir analysis/my-wav-table/replay
```

Reproduce the complete generated-WAV experiment and numerical comparisons:

```sh
rtk proxy python3 tools/test_wav_to_virus.py
rtk proxy python3 tools/characterize_wavetable.py
rtk proxy python3 tools/demo_wav_import.py
```

## Evidence and boundaries

REA skill workflow: static shipped-code tracing plus controlled process replay of the dedicated DSP emulator. The connected REA native provider does not directly support this raw DSP image. See [harness provenance and substitutions](../native/dsp-probe/README.md).

The [REA bundle](../analysis/wavetable-trace/wav-import/rea-wav-import-evidence.json) includes:

- Basis capture: `ev_c060f3e18cbd3a93a350a215c9c0cbcf69147bbed812c4dc5a349484bd5d96ce`.
- New-WAV end-to-end capture: `ev_9fbb21d071bc550a906b57e59db8f2a4d7c6c8f6e48058054714df9ed9e18d3a`.

Both exited successfully and retained complete raw terminal output. REA omitted some cumulative rendered-terminal snapshots due to their output budget; that truncation is retained in the evidence. Input, tool and generated-image hashes are in the output manifests. Process sampling, inherited environment, non-atomic executable identity and unobserved filesystem effects remain REA limitations.

This establishes the converter and original synthesis routines under the existing emulator's memory model and allocation/timing substitutions. Device boot, hardware integrity checks, updater acceptance and physical flashing remain untested. The complete oscillator playback/interpolation path, other descriptor sizes and flag meanings are outside this result. The supplied installer and original firmware images remain unchanged.
