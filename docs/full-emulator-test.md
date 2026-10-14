# Full OsTIrus note and wavetable-sweep test

**Passed. The modified firmware boots in the OsTIrus TI engine and plays the new Prism Formant wavetable through the complete voice path.** It responds to notes, chords and wavetable-position changes. An untouched table is byte-identical between the original and modified firmware under the same test sequence.

## Listen

- [New table: full-engine notes, position steps, sweep and chord](../analysis/full-emulator/Prism-Formant-full-engine-demo.wav), 13.5 seconds.
- [A/B: original HarmncSweep, pause, then Prism Formant](../analysis/full-emulator/HarmncSweep-then-Prism-Formant-AB.wav), 8.8 seconds.
- [Unedited complete modified render](../analysis/full-emulator/modified-id1.wav) and [original render](../analysis/full-emulator/original-id1.wav), 15 seconds each.

These WAVs contain output from the OsTIrus engine playing MIDI. They are distinct from the earlier previews made by interpolating extracted cycles in Python. The A/B has short audition-only edge fades; the full 15-second renders have no normalization or fades added.

## What ran

The local host links the [Gearmulator/OsTIrus engine](https://github.com/dsp56300/gearmulator/tree/897b31893b3a7c1a98aaa3909e57c785cb6ade82) at commit `897b31893b3a7c1a98aaa3909e57c785cb6ade82`, using its pinned DSP56300 JIT implementation. It selects **TI mode with two DSPs**, loads the firmware, waits for boot, and feeds MIDI through the ordinary `virusLib::Device::process` API. It does not replace the synthesis routines with stubs or feed pre-rendered waveforms into playback.

OsTIrus models the microcontroller and hardware interfaces in its own code; this is the full OsTIrus synthesis engine, not a claim that every TI hardware chip or controller instruction is emulated. [Host source/build details](../native/osti-render/README.md).

Its loader expects the outer `FORM/TABL` firmware bundle. The test therefore substitutes the modified `vti.bin` member into a copy of the original `firmware.bin`. The copy differs in exactly 386 bytes: 384 spectral coefficients and two F029 CRC bytes. All other members and all container lengths remain identical. [Container verification](../analysis/full-emulator/container-manifest.json).

The test patch derives from the installer's factory ` -INIT-   ` patch. It selects oscillator 1 in wavetable mode, turns off the other oscillators, effects, modulation slots and velocity modulation, opens the filters, and holds the amplitude envelope. The ID 2 control changes only the table selection and patch name. [Parameter/source plan](../analysis/full-emulator/parameter-plan.md), [patch provenance](../analysis/full-emulator/init-patch-provenance.json).

## Test sequence and results

Four 15-second renders use the same event timing:

| Firmware | Table | Result |
|---|---|---|
| Original | ID 1, HarmncSweep | Booted and rendered |
| Modified | ID 1, Prism Formant | Booted and rendered; audio changed |
| Original | ID 2, Glass Sweep | Booted and rendered |
| Modified | ID 2, Glass Sweep | **Every float32 audio sample matches the original ID 2 render** |

Each sequence includes:

1. Twelve held positions across CC17 values 0–127, under MIDI note 45.
2. A held note with a two-second forward sweep and two-second reverse sweep.
3. Separate MIDI notes 45, 57 and 69 (nominal A2, A3, A4).
4. A three-note C-major chord, MIDI notes 48/52/55, with another position sweep.
5. Note-offs, all-notes-off, and a request to read the final edit-buffer patch back.

Observed checks:

- All four boots and renders completed normally. Each produced 720,000 stereo frames at 48 kHz.
- All samples are finite and unclipped. The new-table demo peak is **0.15321 full scale**. The final 200 ms are exactly silent.
- MIDI readback confirms oscillator mode 2, the intended table ID, oscillator balance 0, and the final commanded position 126.
- The twelve stationary spectra progress monotonically through the new table. Best-fit cosine similarity against the converted source harmonic magnitudes is **0.9457–0.9946**. This allows overall gain but does not compensate for filters or oscillator interpolation; it is not a sample-identical comparison to the earlier isolated cache.
- In sliding spectral measurements, time versus inferred table position correlates **+0.9987** during the forward sweep and **−0.9996** during the reverse sweep.
- Measured note fundamentals are **110.121, 220.207 and 440.367 Hz**, within 1.91 cents of their nominal pitches. All three chord fundamentals are present simultaneously.
- A second complete run reproduced the same audio hashes for all four cases.

Full numerical results and assertions: [validation.json](../analysis/full-emulator/validation.json). Inputs, event timelines, hashes and execution logs: [run-manifest.json](../analysis/full-emulator/run-manifest.json). The spectral fit checks the direction and identity of the morph against the new table; it does not assert exact transfer-function equivalence to physical hardware.

## Reproduce

After building the host, run from the workspace root:

```sh
rtk proxy python3 tools/prepare_osti_firmware.py
rtk proxy python3 tools/make_full_emulator_patch.py
rtk proxy python3 tools/run_full_virus_test.py
rtk proxy python3 tools/analyze_full_virus_test.py
```

The analyzer uses NumPy, already present in the tested `/usr/bin/python3` environment. Other scripts use the Python standard library. Omit `rtk proxy` if RTK is unavailable.

The prepared [outer bundle](../analysis/full-emulator/firmware.Prism-Formant.experimental.bin) can be used as the ROM input to this OsTIrus host. The inner [modified vti.bin](../analysis/wavetable-trace/wav-import/prism-formant/vti.Prism-Formant.experimental.bin) remains the firmware research artifact; the two files have different container formats and must not be confused.

| Artifact | SHA-256 |
|---|---|
| Modified inner vti.bin | `e219474ac54c00a1be9c982f5649929991e1d3932b490d4584720636467d2cbc` |
| Modified outer bundle | `236e040d1528e5695a1787c9b0922406c7ec9967dbf6e54fb1b5fd0faa483f74` |
| Tested host executable | `8eac9ae3db359d207ffd451cf190bba1706727c8191a378cb7fa92bf5b488c83` |

## Investigation notes and limits

The first host version requested 48 kHz at construction but missed the explicit post-boot sample-rate command. The engine's initialization selected 44.1 kHz internally, producing incorrectly pitched 48 kHz exports. The host now calls `setSamplerate(48000)` after construction. Those preliminary outputs are retained under `initial-rate-mismatch`; all results and listening files linked above come from corrected reruns.

REA's process capture observed the repeated four-render run, including exit code zero and complete output/audio hashes, but returned **`cleanup_incomplete`** when it could not read process-ownership tokens for macOS processes. A read-only process check identified the listed surviving processes as system `mdworker_shared` indexers; none were terminated. Because the capture did not finish its cleanup verification, it did not register a normal Evidence ID. The [complete returned error and partial capture](../analysis/full-emulator/rea-capture-cleanup-error.json) are retained, rather than presenting it as successful formal REA evidence. Independent direct execution, full logs, audio hashes and numerical checks establish the results above.

The [REA bundle](../analysis/full-emulator/rea-full-emulator-evidence.json) preserves earlier successful evidence from the preceding investigations; it does not contain a successful Evidence record for this final four-render capture. Runtime provenance is instead tied to the local manifests and retained partial capture. No unrelated system processes were modified to resolve the tool limitation.

This test exercises OsTIrus boot, MIDI delivery, complete voice generation, wavetable playback, position changes and mixing. It does not qualify the image for physical flashing, establish device-side integrity acceptance, or test every patch, sample rate, voice limit or TI model. The original installer and original firmware remain unchanged; no physical device, updater or audio/MIDI driver was used.
