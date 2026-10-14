# Virus TI firmware and wavetable feasibility

Initial inspection 2026-10-11. Target: the local Virus TI Software Suite 5.1.7.00 installer. **All three firmware images are extracted and their bank encoding is decoded.** This document preserves the first-pass findings. The [subsequent wavetable investigation](wavetable-replacement.md) resolves the ID-to-spectral-data mapping, exports complete HarmncSweep waveforms, and verifies a same-size replacement using original firmware routines in DSP emulation. Additional slots and hardware acceptance remain unproven.

## Extracted images

| Image | Bytes | Decoded 32 KiB banks |
|---|---:|---:|
| [vti.bin](../analysis/firmware-images/vti.bin) | 3,083,108 | 86 |
| [vti_2.bin](../analysis/firmware-images/vti_2.bin) | 3,369,804 | 94 |
| [vti_snow.bin](../analysis/firmware-images/vti_snow.bin) | 1,900,058 | 53 |

The [extraction manifest](../analysis/firmware-images/extraction-manifest.json) records source offsets, sizes and SHA-256. The supplied installer, outer firmware bundle and extracted images remain unchanged. No vendor code, updater or hardware-facing operation was executed.

## What is inside vti.bin

The file is a nested `FORM` container, not one directly executable CPU image. Its 86 chunks each have 35,842 encoded bytes. Each contains 1,024 records of three header bytes plus 32 data bytes, followed by a two-byte trailer. Stripping the record headers produces exactly 32,768 bytes per bank. All record addresses and container lengths were checked.

REA decompilation of the shipped `VirusFirmwarePack::LoadTIOSChunk` confirms this decoding loop, acceptance of 0x8000/0x8c00/0x8c02 chunk sizes, and the bank mapping: `F`/`P` tags add 128 to their decimal suffix; `S` tags do not. The output manifest records this **host loader mapping**, not an independently verified physical flash map.

In `vti.bin`, 32 F banks total 1 MiB, containing controller code/data and the DSP payload; 54 P banks total 1,769,472 bytes of preset-related data. F tags are F000–F013, F020–F035 and F060–F061. Concatenated F output omits the tag-number gaps and must not be treated as a linear physical address map. Individual decoded banks, preserved headers/trailers, strings and hashes are in [decoded/vti](../analysis/firmware-images/decoded/vti/manifest.json).

The public Gearmulator [ROM reader](https://github.com/dsp56300/gearmulator/blob/897b31893b3a7c1a98aaa3909e57c785cb6ade82/source/axel/virusLib/romfile.cpp) supplied a DSP decoding model, which was validated against these bytes: F020–F034 contain 15 sequential DSP blocks, IDs 14 down to 0, with packed big-endian 24-bit words. The first 71 bootstrap words load at DSP word address 0x100. The remaining 163,097 words form a loader stream.

Using the emulator authors' [documented loader commands](https://theusualsuspects.io/technical/osirus-c), this image parses into 16 bounded load records and a jump to P:0xD0C. One additional 0xD0C word follows the jump; its purpose remains unresolved. The [DSP map](../analysis/firmware-images/decoded/vti/dsp-load/map.json) records every command, destination, length and original source location. External X/P memory aliasing remains a caveat: a record loading X memory can contain executable code. TI and TI2 primary DSP command streams are byte-identical in this installer; Snow differs. This does not imply interchangeable complete firmware images.

## Wavetable evidence

**Selection metadata is located.** The shipped `remotedevice.xml` exposes 100 named tables, indices 0–99, with separate WaveTableIndex controls spanning 0–127. The latter range is a control range, not proof of 128 stored frames. All 100 labels occur in decoded F banks. The contiguous label list is in F011: `Sine` at offset 0x2D0A, `HarmncSweep` at 0x2D12, `Glass Sweep` at 0x2D20, and `Domina7rix` at 0x31C4. The [complete label map](../analysis/firmware-images/decoded/vti/wavetable-name-offsets.json) links matches back to original image offsets. Labels are not sample pointers.

**A sine lookup sequence is located.** A loader command writes packed 12-bit values to Y:0x40082. Its first 64 source words unpack into 128 signed values matching `-2048*sin(2*pi*n/128)` to within one integer unit, with range −2048 to 2047. Source: decoded F020 byte offset 630 (0x276), starting at original `vti.bin` byte offset 502,606 (0x7AB4E). The [CSV](../analysis/firmware-images/decoded/vti/dsp-load/sine-lookup-128-signed12.csv) retains these values. This establishes sinusoidal data, **not** that the sequence is wavetable selection 0, nor that the 100 wavetables share this representation. The oscillator reference to this lookup has not been traced.

**The first pass did not resolve the sample directory.** The [follow-up](wavetable-replacement.md) locates it at DSP word address `0x5FF2B`, maps IDs 1–99 to spectral records, and executes ID 1's generation code. Full oscillator playback/interpolation and safe storage expansion remain open.

## Integrity and modification constraints

REA recovered the host updater's complete bank-comparison CRC routine: initial value 0xFFFF, exactly 0x8000 decoded bytes, and the shipped two-table update rule. The [table bytes and formula](../analysis/firmware-images/updater-crc-tables.json) are retained. The separate `updCRC` function uses polynomial 0x1021, but **that is not the function called by GetMemoryCRC**; do not substitute it.

Empirical [CRC checks](../analysis/firmware-images/crc-verification.json) match 85/86 vti trailers, 41/42 TI2 trailers that exist, and 52/53 Snow trailers. F000 is the exception in every image; its discrepancy remains unresolved. The TI2 P064–P115 chunks have no trailer. All primary DSP F020–F034 trailers match the recovered bank CRC.

The loader functions examined unpack bytes without decrypting them or checking a cryptographic firmware signature. `ApplyFLSHSignature` handles an ASCII FLSH metadata marker, not a demonstrated cryptographic signature. The write path erases bank pairs and writes/verifies bank data. This is bounded evidence about the host updater, **not proof that hardware BIOS, boot code, or other paths accept arbitrary modifications**. The updater also preserves device information in a bank buffer, so blindly replacing a full flash image is not equivalent to its update procedure.

## Can we add new wavetables?

**Replacing one existing table, retaining its index and storage footprint, is the most focused first experiment.** The container and DSP payload can already be decoded losslessly; the primary DSP-bank CRCs are reproducible; the next missing piece is the oscillator's table lookup and sample representation. A replacement should first be exercised in an emulator with comparison renders before any hardware experiment.

**Adding additional selectable slots is a broader change.** It may require expanding DSP descriptors and selection bounds, finding storage, changing the controller's name/range handling and updating host parameter metadata. The existing XML caps selection at 99. Editing names or raising that cap alone would not create new sound data. No unused slots or safe spare storage have been established.

The follow-up completed the directory trace, complete ID 1 export, and same-size replacement with isolated generation-code replay. The full WaveTableSelect MIDI path and complete oscillator playback remain outside that validation. A hardware-qualified modification has not been produced.

## REA evidence and limitations

REA's Linux Binwalk/Unblob firmware tools were unavailable on this macOS host. REA/Ghidra instead analyzed the shipped updater's x86_64 slice; custom offline parsers decoded the firmware using that evidence and public format references. The DSP firmware itself was not decompiled by REA. Decompiler output is recovered pseudocode, not source; some C++ signatures/argument types are imperfect.

Full evidence: [rea-firmware-updater-evidence.json](../analysis/rea-firmware-updater-evidence.json). Resumable analysis: [UpdaterPlugin-firmware.rea-snapshot.json](../analysis/UpdaterPlugin-firmware.rea-snapshot.json). Updater slice SHA-256: `8f2871a8aa4ab8d7d29e0d3d3b724872256b9504a4ecfcff79ff599d466424a8`.

| Observation | Function/address | REA evidence ID |
|---|---|---|
| Package member selection | Load / 0x1DCC8 | ev_5d62d780ba8fddad0dd395b609cbcffc854ec965b678424fabbfff288378d110 |
| Record decoding and bank mapping | LoadTIOSChunk / 0x1DA48 | ev_e295405d59f852ef9658c3069d89a8ff25631a4938a3c76caa7caa3452efd10a |
| CRC seed and byte count | GetMemoryCRC / 0xC044 | ev_eca1bbefb4732dd14b498bb0920eb72021988f17bc2bfbdb6a3098d479509f8e |
| Actual CRC update rule | updateCRC16 / 0xBF08 | ev_b4d435de5ecca2456462d4169cc7273d7a783429d6783bdd53045027cde0f493 |
| CRC tables | bytes at 0x3E140 | ev_ae55bfb666c009fcdf7d45024b5b452b5efcac04556943310ae6ee51d25dc234 |
| Bank verification | CompareFlashBank / 0x125E8 | ev_bfa5296574007e355d6e566992385c084c99345f8ccb8371fcd44ec0d27b8565 |
| Bank-pair write path | PerformWriteFlashPacks / 0x12F92 | ev_56ddc07689db5a61e0799a2fee0e36ddcb152758ba7c68c564fb9bc66e27824a |
| FLSH metadata marker | ApplyFLSHSignature / 0x11C0E | ev_6b6faf5d963c02bd266364a599593060aad7a8d2499c9c546df65d59718d34b6 |

## Reproduce the offline extraction and mapping

```sh
rtk proxy /usr/bin/python3 tools/inspect_firmware.py \
  --bundle 'analysis/package-expanded/Core_components.pkg/Payload/Library/Application Support/Access Music/Virus TI/Common/firmware.bin' \
  --names-xml 'analysis/package-expanded/Core_components.pkg/Payload/Library/Application Support/Access Music/Virus TI/Common/Virus Control.bundle/Contents/Resources/remotedevice.xml'
rtk proxy /usr/bin/python3 tools/map_dsp_load.py \
  analysis/firmware-images/decoded/vti \
  analysis/firmware-images/decoded/vti_2 \
  analysis/firmware-images/decoded/vti_snow
```

If RTK is unavailable, omit `rtk proxy`, as in the workspace README. Validation passed for every bank of all three images: bounded container parsing, unique destinations, all record counters/addresses and byte-identical reconstruction from saved raw banks plus headers/trailers. Deliberately truncated input and a corrupt record address were correctly rejected. DSP load records were bounded and exported for all three images. No runtime or hardware acceptance claim follows from these offline checks.
