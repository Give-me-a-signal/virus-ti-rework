# Bitwig hardware validation — 12 October 2026

Host: licensed Bitwig Studio 5.2.7, native arm64 plug-in host, 44.1 kHz / 256 frames. TI Desktop capture remains 48 kHz, six channels. Evidence is in `analysis/bitwig-validation-2026-10-12/`. Only the dedicated `Virus TI Hardware Recall 2026-10-12` project was edited.

**Result: native Bitwig save/reopen passed.** The project automatically reconnected in a fresh plug-in process. All sixteen hardware sound dumps, multi sound/routing and 2,672 saved parameters matched; all sixteen parts sounded before and after reopening, with zero capture errors or FIFO overruns/underruns. The TI is now disconnected and its pre-test RAM sounds have been recovered and verified. No firmware or stored banks were written.

Saved project: `~/Documents/Bitwig Studio/Projects/Virus TI Hardware Recall 2026-10-12/Virus TI Hardware Recall 2026-10-12.bwproject`. SHA-256: `c0c4c5f4d03580667f7969f5a90523736cfcdbf350bb583d9b87ed80a55ef579`. Summary: `verification.json`.

## Save timeout correction

The previous connected project snapshot read every part twice, taking about 33 seconds. Bitwig rejected that save with its plug-in state timeout dialog. The incomplete save was cancelled.

A batched-request experiment failed on physical hardware and was removed. A separate read-only probe measured approximately one second per serial patch read on both public and private request paths. Bursting sixteen requests returned a truncated response and only the last complete part. Logs: `read-timing.log`, `read-public-timing.log`, `batch-timing.json`. During the experimental disconnect, the failed snapshot was reported; all original part, Single and multi restoration files were nevertheless produced. This is not counted as a successful snapshot.

The final code requests each of the sixteen parts once, sequentially. Both protocol checksums, part identity and patch parsing remain enforced, followed by the multi checksum. The complete temporary snapshot replaces saved state only after every read succeeds. Preset loading retains its separate double-read verification. Saves are fresh reads, not reuse of an older cache. A save spans multiple hardware reads; controls should remain unchanged during that interval. It is not an instantaneous global snapshot.

The rebuilt `native/vst3/build-bitwig` passes five CTest suites and 47 Steinberg validator tests (`ctest-final.log`, `validator-final.log`). The installed development VST3 symlink targets that build.

## Host cache invalidation correction

The serial-read build passed the API snapshot and all sixteen auditions, but Bitwig subsequently saved without requesting fresh component state: the service's completed request count remained unchanged at 34 through Save and Save As. Those files therefore do **not** establish connected-state recall. `native-save.json` is a failed file-time observation, not proof of a successful fresh snapshot.

API changes bypass controller `performEdit()`. The final build now tracks state revisions for connection, preset loading, cutoff, routing and API project loads. A controller timer polls that revision on the main run loop independently of the editor and calls VST3 `IComponentHandler2::setDirty(true)` when it changes. Notes, status queries and snapshots do not increment it. Hardware recall notifications are deferred until recall completes so the host does not immediately attempt a snapshot while the long recall is still running.

A regression test with a mock component handler, with no editor created, confirms one dirty notification per changed revision and none for repeated status responses. Other tests check that invalid presets do not advance the revision and that saving does not dirty the state again. These tests pass, and the final build passed the native Bitwig save/reopen described below.

## DAW and hardware evidence

- The native Bitwig clip contains G2, C3 and E3. Playback through the TI instrument track produced hardware MIDI, TI USB capture and visible instrument/master audio meters. `daw-midi-playback.json` records 12 MIDI messages, part activity up to 0.7344 and USB1 peak 0.08078, with zero capture/FIFO errors. This clip was played in Bitwig, not in the bench host.
- The original Access artwork and Modern Dark appearance were opened in Bitwig; the editor was scaled to 125%. “Original Access” is the reconstructed editor using original artwork, not the vendor binary.
- `before-save.json` verifies all sixteen MATRIX presets sound through the hardware, each part's activity responds, and USB audio returns. Capture errors, FIFO overruns and underruns remained zero.

All sixteen audition results also show activity only on the addressed part and the largest audio peak on its assigned USB bus. After release, every activity value was zero and the largest USB meter reading was below 0.000001. This is a meter-based release check, not a recorded waveform analysis.

## Completed native save/reopen

After unlock, Finder could open projects, but Bitwig's own CUA binding continued returning `timeoutReached`. The user therefore performed the native Cmd-S and quit actions and confirmed saving succeeded. `manual-save-result.json` records the changed file timestamp, size and hash. No API state-load was used to reopen or supply the recalled state.

The pre-save plug-in process was PID 66259. Reopening the exact saved project through Finder launched PID 68810 and automatically began hardware connection/recall; no API connect command was sent. `after-reopen.json` and `after-reopen-test.log` pass the comparison against `before-save.vtrstate`: all sixteen sounds, multi sound/routing and every one of the 2,672 parameter ID/value tuples match. The comparison excludes documented management/header/checksum fields from sound identity, rather than claiming every metadata byte is identical.

Thirty-two isolated hardware auditions (sixteen before save and sixteen after reopen) passed. Each showed activity only on its addressed part and its largest audio peak on the assigned USB bus. All activities decayed to zero and final USB meter readings were below 0.000001. Both sessions recorded zero capture errors, FIFO overruns and underruns. These tests used the actual VST3 hosted in Bitwig.

The earlier `Virus TI Connected Recall.bwproject` was created before the cache notification fix and is not the passing project. The successfully saved hardware recall project's hash remained unchanged during final cleanup.

## Shutdown limitation and recovery

The native quit did not complete original-RAM restoration: no `restored-*.syx` files were produced in session `94B1FCA4-C0AB-4D92-8B5B-A4187B5DCABA`, and all sixteen part sounds at the next connection differed from that session's original sounds (`quit-restoration-observation.json`). This establishes a quit-time restoration failure; it does not establish the precise host/plugin lifecycle cause.

**Resolved in the subsequent `build-restoration` update:** normal Bitwig Cmd-Q now automatically restores and verifies all sixteen original sounds, the Single buffer and multi routing. The prior failure above remains historical evidence. See [automatic quit restoration and hardware tests](quit-restoration-2026-10-12.md).

The reopened session was explicitly disconnected through the API (`after-reopen-disconnect.json`). Since its baseline already contained the previous test sounds, a recovery helper then restored the actual pre-test snapshots from the prior session. It acquired the same exclusive hardware lock, validated every saved patch/checksum before sending, used only the existing allowlisted RAM/mode/routing commands, and verified all sixteen readbacks, the Single buffer and multi sound/routing. `final-ram-restoration.json` and `.log` pass. Its source and raw USB traffic are retained under the validation directory. The Single buffer was byte-identical; only documented management metadata differences are permitted for part/multi comparisons.

Final hardware state: disconnected, original sixteen RAM sounds restored, Single mode sent and USB configuration normalized to two returns. The saved project's connection intent remains enabled on disk. The still-open Bitwig instance is disconnected after cleanup; saving it again would save that disconnected intent.

`final-midi-audit.json` covers 536 transmitted messages from two earlier diagnostic sessions; `reopen-and-recovery-midi-audit.json` covers another 387 from the reopened session and final recovery. All audited messages stay within the existing read/note/RAM-patch/routing allowlist. The quit-terminated session did not finalize its raw traffic files, so no complete capture audit is claimed for that session.

## Boundaries

The sixteen part strips show note-velocity activity separately from their CC7 volume faders. The three USB meters show actual mixed audio peaks; no individual-part audio dB telemetry is claimed. See `part-meter-evidence.md`.

No Bitwig audio recording or exhaustive click analysis has been produced in this run. Earlier six-channel recording tests are documented separately in `hardware-matrix-2026-10-12.md`. Pitch bend, pressure, program changes and other documented reconstruction gaps are not validated here. Bitwig logged a warning about an automatable parameter also being advertised as a MIDI parameter; ordinary note playback works, but that warning is still unresolved.

No firmware flashing or stored-bank writes are part of this workflow. Disconnect restores original RAM sounds and sends Single mode, with USB configuration deliberately normalized to the standard two-return setting. Exact restoration of unknown global settings is not claimed.
