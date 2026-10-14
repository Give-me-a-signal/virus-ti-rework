# Sixteen-part presets and project recall

The development plug-in now loads presets independently into all sixteen TI
parts and saves their complete RAM patches plus the multi in VST3 component
state. This was tested through its private API using the full OsTIrus TI engine,
without opening the physical TI or transferring firmware.

## Implementation

- Browser selection and API `--part 1` through `--part 16` target the corresponding
  RAM edit buffer. Loading one part preserves the other fifteen patches and
  the multi’s output routing. Bank imports remain local; choosing an entry
  loads only that preset into the selected part.
- MIDI channels 1–16 address the sixteen parts. The API can select each part’s
  USB1, USB2 or USB3 stereo return. All sixteen parts are enabled on fresh
  connection; restored projects use their saved multi.
- State version 3 stores sixteen independently checked patches, the multi,
  connection intent and all 2,672 mapped controls. A full state is 40,871 bytes.
  Version 1 parameter states and version 2 Part 1 states remain readable.
- Saving queries two stable, valid dumps per part and a validated multi. A
  failed snapshot fails the save instead of substituting stale patches.
  The worker continues servicing MIDI while waiting for replies.
- Restore validates the entire state before applying anything. Disconnect
  retains the project separately while restoring the original edit buffers.
  No stored-bank write or firmware command is admitted by the shared MIDI
  allowlist.

## Emulator test

`vst3_api_host` runs the real processor’s `process`, `getState` and `setState`
callbacks at 48 kHz with three stereo outputs. The optional backend uses the
pinned Gearmulator `virusLib` TI mode and both emulated DSPs. The ROM is the
original installer’s outer `firmware.bin`, SHA-256
`8e3daea8552f9ecb1b7205bb5f81e7391dcf406e86af81fc7124dba00fb5cd6a`.
See [engine provenance](../native/osti-render/README.md).

The API test:

1. Imports a generated sixteen-entry bank, loads each entry into its own part,
   and compares every other part and the multi after each load.
2. Edits each part’s cutoff independently, assigns returns round-robin across
   USB1–3, then saves the complete project.
3. Plays each MIDI channel separately and checks that only its selected stereo
   return contains audio. Plays all sixteen parts together and checks finite,
   nonclipping output on all three returns.
4. Changes every part and route, recalls the saved state, then verifies every
   sound and the complete multi. Corrupts the final multi CRC and verifies
   rejection without partial application.
5. Disconnects, saves the retained offline sounds, and automatically reconnects
   through project recall. Checks all sixteen sounds and the multi again.
6. Terminates the host, starts a fresh process, selects the emulator, and loads
   the saved project. Checks all sixteen sounds and routing, plays every part,
   and checks silence after each note release.

“Sound match” compares all patch bytes except the device/address fields,
patch-management/original-bank metadata and checksum bytes. The complete multi
is compared byte-for-byte. These tests use current-format patches derived from
the stock low-volume wavetable probe, with independent names, positions and
cutoffs; they are not an exhaustive audition of every factory preset.

Machine-readable results, counters, audio measurements, executable hashes and
the MIDI audit are in
[summary.json](../analysis/daw-validation/multipart/summary.json).
Detailed runs are [roundtrip.json](../analysis/daw-validation/multipart/roundtrip.json)
and [restart.json](../analysis/daw-validation/multipart/restart.json).
The [saved project](../analysis/daw-validation/multipart/all-parts.vtrstate)
is self-contained; its bank file is unnecessary for recall.
[Listen to the sixteen recalled parts](../analysis/daw-validation/multipart/sixteen-recalled-parts.wav).

The final native suites pass 4/4 and the Steinberg validator passes 47/47.
Tests include legacy state compatibility, truncated-state rejection,
controller recall for all sixteen parts, instance isolation and a suspended-host
regression for the larger emulator FIFO.

Both final API runs reported **zero FIFO underruns, overruns and capture
errors**. All sixteen post-restart release tails were exactly zero over the
last 100 ms checked. The simultaneous sixteen-part chord peaked at 0.2295,
0.1834 and 0.1834 full scale on USB1, USB2 and USB3. Three complete emulator
sessions contributed 1,863 audited outgoing messages, with zero firmware
transfers and zero stored-bank writes. The recalled-part demonstration is
17.84 seconds long.

## Use away from the hardware

The [API instructions](../native/vst3/README.md#testing-without-the-ti-connected)
show how to start the host, select the emulator ROM, load individual parts and
save/restore projects. Always select the socket for that host’s PID when Live
is also running. No visible editor or physical device is required.

Backend selection and the ROM path are deliberately external to project state.
Select `emulator` before loading a connected project into a fresh test host.
The emulator never falls back to physical hardware. The headless host writes
six-channel float32 audio to disk; it does not play directly to speakers.

## Limits and observations

- This validates the new sixteen-part implementation in the emulator. Physical
  parts 2–16, USB driver behavior and USB dropouts still require the real TI.
  The older plug-in already loaded in Live and its installed symlink were left
  unchanged. The new candidate is in `native/vst3/build-multipart`.
- Live’s current licence prevents `.als` saving. This exercises the actual
  plug-in state callbacks, including a full process restart; an actual Live
  set save/reopen remains untested.
- The optional emulator has an 8,192-frame FIFO (about 171 ms at 48 kHz) to
  absorb JIT/CPU scheduling pauses. Hardware retains its 2,048-frame FIFO.
  An earlier run recorded one underrun during a silent initial snapshot; the
  final-run counters are reported explicitly in the summary. This is not a
  guarantee of glitch-free emulator operation under arbitrary CPU load.
- OsTIrus factory RAM starts with some version-8 patches. Reloading them makes
  the running firmware convert them to version 12. These original-buffer
  restorations are logged and checked for stable valid readbacks and matching
  names, rather than claimed as byte-identical. Saved current-format project
  sound data and multi restoration are checked separately as described above.
- Active voices, effects-tail memory, every global hardware setting and legacy
  vendor plug-in project chunks are outside this state format. Emulator use
  inside Live, including dynamic latency compensation, needs separate DAW
  validation; this turn’s emulator tests use the API host.
