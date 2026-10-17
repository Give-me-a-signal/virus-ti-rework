# TI-family discovery audit — issue #8

## Scope and result

This audit records the detection-only change and its sixteen-part baseline.
The consolidated [four-part follow-up](snow-four-part-recall.md) now enables the
ordinary Connect TI action for Snow in PR #9, with model-specific preflight and
recovery checks. The detection-only refusal described below is historical;
Snow remains unverified on physical hardware.

[Issue #8](https://github.com/Give-me-a-signal/virus-ti-rework/issues/8) reports
`Virus TI Snow` in macOS Audio, while Rework says USB audio is absent. The
published 0.2.0-beta.1 source matches only the exact audio name `Virus TI`.
Its MIDI discovery similarly matches only generic TI port names. This explains
the reported audio-name failure from source inspection; it has not been
reproduced with a physical Snow here.

The fix shares exact, ASCII case-insensitive family/port classification between
CoreAudio and CoreMIDI. It accepts the existing generic name and TI/TI2
Desktop/Keyboard/Polar aliases, recognizes Snow separately, rejects ambiguous
device/port matches, and gives actionable missing-MIDI diagnostics.
The historical hardware bench also uses the shared selector and records the
actual device name instead of relabelling every device `Virus TI`.

**Historical detection-only behavior:** Snow was unverified and blocked. Recognition did not
enable playback: our snapshot, part-write, project-recall and restoration paths
assume sixteen parts. Snow has four. Snow is rejected during audio selection,
before opening MIDI, creating capture callbacks or sending device commands.
The service may already have acquired its lease and created its recovery
directory; this is not a claim that Connect performs no local bookkeeping.

## Model coverage

These are discovery rules, not a new hardware support list. The generic
`Virus TI` name does not identify a particular form factor or generation.

| Model / variant | Accepted audio label in this fix | Evidence and remaining scope |
| --- | --- | --- |
| TI Desktop | `Virus TI`, `Virus TI Desktop` | Generic name used in our TI Desktop baseline; Desktop string also appears in the legacy MIDI binary. No new hardware test in this change. |
| TI Keyboard | `Virus TI`, `Virus TI Keyboard` | Keyboard string appears in the legacy MIDI binary; runtime audio/MIDI/recall/recovery unverified here. |
| TI Polar | `Virus TI`, `Virus TI Polar` | Polar string appears in the legacy MIDI binary; runtime audio/MIDI/recall/recovery unverified here. |
| TI2 Desktop | `Virus TI`, `Virus TI2`, `Virus TI2 Desktop` | TI2 spellings are defensive aliases, not observed CoreAudio names. Transport and recovery unverified here. |
| TI2 Keyboard | `Virus TI`, `Virus TI2`, `Virus TI2 Keyboard` | Defensive aliases; transport and recovery unverified here. |
| TI2 Polar | `Virus TI`, `Virus TI2`, `Virus TI2 Polar` | Defensive aliases; transport and recovery unverified here. |
| TI Snow | `Virus TI Snow` | Exact audio label reported in #8 and present in the legacy MIDI binary. Recognized, then refused with an explicit unverified-model message. |
| Limited finish variants (e.g. Darkstar / WhiteOut) | Only the applicable family names above | No evidence of distinct USB names or transport profiles was established. No speculative finish-name aliases or support claims. Request actual descriptors/names if discovery differs. |

The MIDI classifier accepts each listed family label followed by exactly
` USB Plugin I/O` or ` USB External I/O`. This does not create ports or replace
a missing driver. Snow-labelled MIDI ports are recognized by the classifier,
but normal service connection rejects Snow before constructing the MIDI client.
Multiple recognized audio devices or duplicate matching ports are refused;
discovery must not silently select the last matching synth.

Pre-TI models (A/B/C, Classic, Indigo, Rack) and PowerCore do not become eligible
through this change. Unknown labels and prefix/suffix lookalikes remain rejected.
Do not add a broad `Virus*` match to work around a new report.

## USB identity and transport

The shipped Access driver lists vendor `0x133e` with products `0x0815` and
`0x0816`. Our descriptor/probe code already knew both; the private transport
looked only for `0x0815`. It now recognizes both and explicitly refuses
`0x0816` **before opening its interface**, because its transport profile has not
been validated. It no longer hides that case behind a missing-interface error.

Neither ID is mapped to TI, TI2 or Snow on this evidence alone. In particular,
do not assume `0x0816` means Snow or TI2. A descriptor's revision and product
string are useful diagnostics; their model interpretation needs separate evidence.
`0x0815` retains the existing two-vendor-interface, single-location checks.
The six-channel, one-stream, 48 kHz float32 capture requirements are unchanged.
There are no new protocol commands, USB resets, firmware transfers or stored-bank
writes in this patch.

## Evidence and provenance

- [Access product specifications](https://www.virus.info/virusti/specs) and
  [manuals](https://www.virus.info/support/manuals) identify the TI form factors,
  TI2 series and separate Snow documentation.
- Owner-supplied 5.1.7.00 package: **Virus TI Snow Reference**, Multi Mode
  Parameters, printed page 117 (PDF page 67), describes four Parts. The manual
  is not redistributed here.
- Static REA inspection of the package's 10.9 `AccessVirusTI.kext` Info.plist,
  driver 3.3.11, confirms both USB product IDs. SHA-256:
  `9ac14182646106579a71953ffe08e293fa9e1080cdda493125e4a440ed2757b5`;
  local evidence `ev_dd340a81a6e43dc746276af3a615f253e1dd2fb0f0e13dc09486551c7c4340f1`.
- Legacy MIDI x86_64 slice SHA-256:
  `52a173fd23b2345b4d32f9752144c2b66fd33f55a787564c4063ae5c7851534f`.
  String inventory contains generic TI, Snow, Polar, Desktop and Keyboard labels
  (`ev_a0f5fa84be6f0748498c5369d3003a59a51eb5d98385d970e3249ca13682941d`).
  Inspection of the device-construction function shows product-string-based
  naming with a generic fallback
  (`ev_fa10692a47eff072b35b35213dbf0dea95fe7d02b59627f931411a4e2bebeb30`).
  Strings are not proof of present-day CoreAudio names or successful hardware I/O.
- A bounded legacy HAL string inspection yielded no usable model-name list.
  TI2 aliases above are explicit defensive choices, not findings from that search.

All new classifier/test code is project-owned MIT source. No collaborator code,
vendor implementation, decompiler output, binaries, manuals, descriptors with
personal identifiers or patch data are included. Raw evidence stays local.

## Validation and next step

Regression cases cover every listed TI/TI2 label and MIDI role, case variants,
Snow refusal, mixed-device order, duplicate devices, absent/invalid identities,
lookalikes, old models, both legacy USB IDs and rejection of unsupported products.
The portable suite runs normally and with AddressSanitizer/UndefinedBehaviorSanitizer;
the same classifier is used by production discovery and the CMake test target.

Local arm64 Release build targets macOS 12 using the pinned SDKs. All ten CTest
suites and all 47 Steinberg validator tests passed. These are offline results,
not Snow/TI2 hardware validation. See the PR for CI and packaging results.

Before a Snow connection can be enabled, collect the [read-only diagnostics](HARDWARE_TESTING.md#read-only-diagnostics-for-snow-and-other-unverified-models),
establish its descriptor/MIDI transport and four-part edit-buffer/routing profile,
implement bounded model-specific snapshot/recall/restoration, then validate those
paths on a real Snow with owner-captured volatile data. Do not infer support from
its six audio inputs alone. The published beta is unchanged by this source PR.
