# Wavetable IDs, recovered waveforms, and an offline replacement

Inspected 2026-10-11, Virus TI Software Suite 5.1.7.00. **ID 1, HarmncSweep, is traced from the DSP selector to its stored spectral coefficients and through the firmware's waveform-generation routines. One complete table has been exported. An experimental replacement reverses its frame order; replaying the modified image produces the expected reverse ordering bit for bit, across all 12 frames and six band-limited levels.** Physical hardware acceptance remains untested.

The [subsequent WAV-import pass](wav-import.md) establishes the harmonic/phase mapping and validates a completely new WAV-derived table, including a reusable converter.

This continues the [installer and bank investigation](firmware-wavetables.md). Original `vti.bin` SHA-256: `78844fced13ca92d34f6a62806321c0826a742666fa3aa90a6ed40d538ca9d87`. All source images remain unchanged.

## ID to stored data

DSP addresses below are **24-bit word addresses**, not file byte offsets. External P/X/Y memory above `0x40000` is modeled as shared memory. The loader places the relevant block at X:`0x4F000`, including subsequently executed code.

| Stage | Address | Observed behavior |
|---|---|---|
| Selector | P:`0x5FC06` | Reads a packed parameter, shifts right 16, and selects directory entry `0x5FF2B + 2*ID`, bounded to ID 99. |
| Caller | P:`0x590D4` | Calls the selector at `0x5FC06`. The entire incoming MIDI-to-parameter path has not been traced. |
| Directory | X:`0x5FF2B` | 100 pairs of source descriptor pointer and runtime cache pointer. ID 0 has a null source and a default path; IDs 1–99 have stored records. |
| ID 1 entry | X:`0x5FF2D` | Source pointer `0x5FFF3`; adjacent word is its cache pointer. |
| Builder | P:`0x5FC2A` | Scans pending entries, reads the descriptor, allocates and generates the cache. |
| Unpacker | P:`0x5FDB1` | Unpacks three spectral bytes per word and applies the coefficient/gain arithmetic. |
| Transform | P:`0x5FD0D` | Executes the transform used to generate waveform samples. |

Selector replays of IDs 0, 1, 33, 99, 100 and 127 confirmed the directory lookup and upper bound for the tested `ID << 16` parameter representation. See [selector validation](../analysis/dsp-probe-selector-validation.json).

The [directory export](../analysis/wavetable-trace/structures/wavetable-directory.json) parses all 99 stored records. All 98 adjacent record boundaries match the size formula below; all payload bytes are in 0–127, and all padding bytes are zero. Names come from the shipped table-name order.

## Complete HarmncSweep record

The four header words at `0x5FFF3` are `[5, 12, 0, 0x5B7A88]`: log2 bin count, frame count, a flag, and gain. They precede **12 frames × 32 spectral bytes**, occupying 128 packed words. Including its header, this record is 132 words / 396 bytes. The next descriptor begins at `0x60077`.

| Item | Decoded F029 byte offset | Original `vti.bin` byte offset |
|---|---:|---:|
| Header | `0xDEF` (3567) | 828469 |
| First spectral byte | `0xDFB` (3579) | `0xCA441` (828481) |

The encoded file inserts three record-header bytes per 32 data bytes, so the 384 spectral bytes are **not contiguous in `vti.bin`**. The [source byte map](../analysis/wavetable-trace/structures/id001-HarmncSweep.source-byte-map.json) identifies every byte. All 384 mappings were checked against the original image.

Extracted artifacts:

- [Complete packed record](../analysis/wavetable-trace/structures/id001-HarmncSweep.record24be.bin), including the four-word header.
- [Spectral coefficients as CSV](../analysis/wavetable-trace/structures/id001-HarmncSweep.spectral-u8.csv), 12 rows × 32 coefficients.
- [Generated samples as CSV](../analysis/wavetable-trace/replay/original/frames-12x64-signed24.csv), 12 rows × 64 signed 24-bit samples from the first level.
- [Lossless 12 × 64 sample WAV](../analysis/wavetable-trace/replay/original/HarmncSweep-12x64.wav). Its 48 kHz rate is export metadata, not a prescribed oscillator pitch.
- [Complete generated cache](../analysis/wavetable-trace/replay/original/rendered-table.24be.bin), including all six levels, descriptors and cycle guards.
- [Six-second listening preview](../analysis/wavetable-trace/replay/original/HarmncSweep-sweep-preview.wav). This is a derived 220 Hz sweep with linear phase/frame interpolation and short fades; it is not a complete Virus oscillator or patch render.

The stored values are spectral coefficients, **not PCM samples**. For this record's gain `g`, the unpacker's arithmetic matches the following integer calculation for all 384 coefficients:

```text
G = ((g*g >> 23) * 0x517CC2) >> 24 = 0x14CF73
q = signed_8_bit(packed_byte)
squared = min(q*q << 9, 0x7FFFFF)
coefficient = (squared * G) >> 23
```

This formula is validated for the tested record and DSP status; it is not a universal proof for every header/flag combination. [Independent coefficient comparison](../analysis/dsp-probe-coeff-validation.json).

## Executing the actual generation code

REA's native provider rejects this raw firmware format. The investigation therefore combines REA's recovered host-loader/CRC evidence with a dedicated DSP disassembler and instruction emulator, driven through REA controlled process captures. The DSP implementation is pinned to [mborgerson/dsp56300 commit 05c09f7](https://github.com/mborgerson/dsp56300/tree/05c09f74e3c31131645edd919a00689d2ec77c62). Build details and exact substitutions are in the [harness README](../native/dsp-probe/README.md).

The replay loads all 16 decoded DSP blocks, marks ID 1 pending, and executes P:`0x5FC2A` through `0x5FD0C`. It substitutes allocation, free and timing calls. The original firmware instructions perform coefficient unpacking, transformation and cache generation. Both original and replacement runs reach completion after **122,319 instructions**.

The generated cache at `0x80FA3` contains 45 header words plus 12 frame records of 294 words each. Each frame contains levels of 64, 64, 64, 32, 32 and 32 samples, each followed by a guard sample. All 72 guards equal their respective first sample. The 64-sample exports preserve cache order; the descriptor's phase-zero pointer is 32 samples into that cycle, so the export has a cyclic phase rotation relative to that pointer.

The [REA evidence export](../analysis/wavetable-trace/rea-dsp-replay-evidence.json) retains successful and unsuccessful attempts. The final successful captures are:

- Original: `ev_07d43a62f061c0361a391b3603b271c2d38b967017aa0c90da73375b59f4607f`.
- Replacement: `ev_cd5de8c47fe421dc779edff6e975af45c57fdd5e1e093b7899282dd8ec199dea`.

Both have exit code zero and untruncated compact results. Full native output is saved beside each replay's summary. Wrapper output records SHA-256 for the executable, configuration, source image and load blocks. This is controlled emulator evidence, not an independent hardware oracle. REA's process/environment and filesystem-observation limitations remain in the evidence export.

## How the replacement works

[replace_wavetable.py](../tools/replace_wavetable.py) takes a coefficient CSV with the existing table's shape. It checks the exact original image hash, follows the load-stream/bank mapping, changes only coefficient bytes, and recalculates affected bank CRCs with the updater's recovered two-table algorithm. It preserves the descriptor, directory, bank record headers, image length and all unrelated bytes, then re-extracts the coefficients to verify the result. It refuses to overwrite the original. It accepts IDs 1–99 structurally; only ID 1 has received the complete rendering validation described here.

Reproduce the verified experiment from the workspace root (omit `rtk proxy` if RTK is unavailable):

```sh
rtk proxy python3 tools/replace_wavetable.py \
  --id 1 \
  --coefficients analysis/wavetable-trace/experimental/HarmncSweep-reversed.spectral-u8.csv \
  --output analysis/wavetable-trace/experimental/vti.HarmncSweep-reversed.experimental.bin
```

The [experimental image](../analysis/wavetable-trace/experimental/vti.HarmncSweep-reversed.experimental.bin) reverses the 12 original spectral frames. It changes **48 coefficient bytes and two checksum bytes**, all within F029. Its CRC changes from `0x197B` to `0x56E3`. Output SHA-256: `a7259301cd79457a00ed4c2089d5fddfd99b0f2ded1558f1c81be4eea2409d05`. See its [verification manifest](../analysis/wavetable-trace/experimental/vti.HarmncSweep-reversed.experimental.bin.json).

The modified image was decoded again and its DSP blocks replayed. Comparing all 3,528 frame words, including every level and guard, confirms original frame `i` equals replacement frame `11-i` bit for bit. Cache headers also match. [Comparison result](../analysis/wavetable-trace/replay/comparison.json). Rebuilding with the original coefficient CSV produced a byte-identical original image; an out-of-range coefficient of 128 was rejected.

To design a different same-size ID 1 table, edit the 12 × 32 CSV with integer coefficients in 0–127 and run the same tool. The unchanged shape/gain/flag constrain the sound. The [WAV converter](wav-import.md) now implements the inverse magnitude mapping for cycle-aligned input, with explicit phase and bandwidth limits.

## Remaining limits

Replacing data in an existing slot is demonstrated offline. Adding selectable IDs beyond 99 is a separate code and metadata change: the selector bounds IDs and initialization scans 100 directory entries. No spare storage or safe expansion layout has been established.

This work has not validated full synth boot, device-side integrity checks, updater acceptance, physical flashing, the complete oscillator playback/interpolation path, or every other record's flag/gain behavior. The original updater was not run. The experimental image is a research artifact, not a hardware-qualified update.
