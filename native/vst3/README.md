# Native VST3 development plug-in

This directory builds **Virus TI Rework (Development)**, a real Apple-silicon VST3 processor/controller and native Cocoa editor. It uses project-owned C++/Objective-C++ and the pinned official Steinberg SDK. The editor retains the original 907×513 logical layout with vector Dark/Polar themes and proportional 100–200% scaling; see [modern editor notes](../../docs/modern-editor.md). The plug-in now plays a connected TI Desktop and returns its USB audio in **Ableton Live 12.3.5**, without the bench host. New instances connect when **Connect TI** is pressed or the local API requests it. A saved connected project sound reconnects automatically once the restored instance starts realtime processing. The original vendor executable is not loaded, and firmware transfer and stored-bank writes are excluded from the MIDI command allowlist.

## Implemented

- Native arm64 `.vst3`, local ad-hoc signature, deployment target macOS14 for the optional emulator build (hardware-only builds retain macOS12); executed on this Mac only, not all target OS versions.
- Three stereo output buses and16-channel event input. The injected service bridge can deliver six-channel signed PCM to float/double outputs. Disconnected/missing samples are zeroed with correct silence flags. The internal hardware service uses the existing Apple USB audio driver for capture and the private TI MIDI interface for notes and supported CCs.
- Bounded SPSC bridge with exclusive ownership, activation generations, seek/disconnect invalidation, underrun/drop counters, copied note/pressure/CC messages, exact host-block offsets and an explicit plugin-sample clock domain. Real audio engine capture queues can feed it only when a service supplies an independently established sample-clock mapping. No automatic USB-completion→sample-time assumption is made.
- 2,672 catalog parameters across16parts (2,624 recovered numeric definitions and48 read-only softknob placeholders). Each bounded automation point is examined, up to4096points per block; XML-backed ordinary CCs generate copied MIDI. MIDI note events are bounded to4096per block. All sixteen parts now have physical hardware and emulator API/audio coverage, including independent routing and project recall. See the 12 October hardware report for the measured scope. Other parameter encodings, pitch bend, channel pressure, program change and SysEx forwarding remain unfinished.
- A real `IPlugView` native editor with original page navigation and modern vector artwork, controls, modes, popup value lists, host gestures, host parameter readback, sixteen parts, Command-click default and Shift fine movement. [Editor implementation and limitations](../editor/README.md) describe the reconstructed and unfinished behaviors. Nonautomatable local editor modes are explicitly not host-saved or hardware-sent.
- Allocation-free exercised `process()` paths, lock-free atomic parameter storage, no USB, filesystem, logging or locks in the callback. A control worker owns hardware operations. A 64-tap windowed-sinc resampler adapts the TI’s 48 kHz audio to the DAW rate with FIFO-occupancy drift correction. Hardware MIDI is immediate worker-thread delivery, not sample-accurate scheduling.
- Version 3 component state preserves all sixteen RAM patches, the multi (including MIDI channels and output routing), and the 2,672 mapped parameters. Version 1 parameter-only and version 2 Part 1 states remain readable. Hardware snapshots run on the worker; audio stays lock-free and MIDI is serviced while waiting for replies. Invalid state is rejected before application. This does not read legacy vendor project chunks.

Processor identity `5695B49E529D4E17A2855601A10B277E`, controller identity `5BCB58FDB9C442DE9699904C625DCCB1`. Parameter IDs use project policy `0x10000000 + automation.xml Index`. They do not replace legacy vendor identities in existing sessions.

## Build and test

Requires existing Apple Command Line Tools, CMake3.25+, Ninja and Python3. No global installation occurs.

```sh
./native/vst3/bootstrap_sdk.sh
./native/vst3/build_and_test.sh
```

The SDK revision is pinned by `sdk-lock.json` under ignored `third_party/vst3sdk`. The build uses the actual installed CLT version for the SDK's Xcode-version override when full Xcode is absent. It copies original image resources and the extracted catalog into the development bundle. These are local copies from the user's installer; the original vendor executable is never included or executed.

Build-script output: `native/vst3/build/VST3/Release/Virus TI Rework Development.vst3`. The script itself does not install it. The current user plug-in symlink points to the separately built `build-presets-fixed/VST3/Release/Virus TI Rework Development.vst3`; this keeps a rebuild from overwriting a binary already mapped into Live. Restart the DAW after changing the installed build; removing and reinserting the device alone kept the old code loaded in Live. Ad-hoc signing is for local development, not notarized distribution.

## Validation,11 October2026

- Official Steinberg validator: **47 passed,0 failed** (`build-daw-final/validator.log`). This checks the VST3 contract, not USB sound or UI fidelity.
- Current release processor, native editor and capture FIFO tests pass (4/4, including local socket control). FIFO tests simulate 44.1/48/96 kHz hosts with ±300 ppm clock drift. Earlier processor/editor revisions also passed ASan/UBSan; those historical sanitizer results do not validate the new hardware service.
- Processor tests inject nonzero engine-queue PCM into all three stereo outputs, check float/double conversion, exact copied MIDI bytes/offsets, bridge ownership, future frames/partial underrun, seek/disconnect stale rejection, queue overflow, state validation and zero C++ allocations during exercised callbacks.
- Editor tests call a real controller/recording component handler, check parameter gestures/readback and local mode conditions, and render twelve native page surfaces to `build/editor-renders`. The editor has also been attached to Live’s real plug-in window with connected-device playback.
- Current hardware evidence and executable hash are in `../../analysis/daw-validation/validation.json`; older `verification.json` describes the earlier disconnected build. The 24-second stereo recording is `../../analysis/daw-validation/Ableton-live-TI-return.wav`. The user reports clean playback.

For ASan/UBSan reproduce the build with the same CLT override, separate directory, `-DVTR_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug`; build `VirusTIRework vst3_processor_tests vst3_editor_tests capture_fifo_tests`, then CTest.

## Use in Live

1. Start the TI in **Single** mode, with the desired sound selected. Insert **Virus TI Rework (Development)** on a MIDI track.
2. Open its editor and press **Connect TI**. It saves Single/all sixteen part/multi RAM snapshots and enters temporary SEQ mode. A fresh session uses the current Single sound for part 1 and the existing part buffers for parts 2–16, enables MIDI channels 1–16, and initially routes every part to USB1. A restored project supplies its own patches and multi.
3. Play MIDI channel 1 or a MIDI clip. The instrument track receives USB1 stereo; keep the DAW's usual audio output device. The optional **Test tone** button loads the stock reduced-volume HarmncSweep probe into the volatile part-1 edit buffer.
4. To record, route a separate audio track from the instrument track (post-mixer), leave that recording track's monitor Off and record in real time. Offline export does not synthesize or transmit hardware notes/CCs.
5. Press **Disconnect** before unplugging. It sends all-notes-off, restores the saved RAM buffers and all sixteen bank/program references, requests Single mode, verifies the dumps and releases the USB connection.

The current Single sound is read from hardware. Browser recall for the selected part sends a complete checksum-checked patch; verified mapped controls update from readback without emitting an automation burst. See preset and project-state sections below for scope.

Snapshots and logs are under `~/Library/Application Support/VirusTIRework/<session UUID>/`. Connected version 2/3 project states restore their sound and connection intent once realtime processing starts; empty and legacy parameter-only states do not automatically open hardware. The hardware lock permits one connected development instance. Scanning the plug-in does not open hardware.

## Remaining limits

The FIFO reports approximately 42.7 ms of buffering at the host rate; this is not a measured round-trip latency. Timing still includes hardware/driver latency and immediate, unscheduled MIDI worker delivery. Pitch bend, channel pressure, hardware poly-pressure, complete live parameter synchronization, physical validation of all sixteen parts and all three sounding USB buses, and automatic unplug recovery remain unfinished. The emulator exercises sixteen-part persistence and all three returns. Its larger FIFO reports about 170.7 ms; that optional backend is currently validated in the API host, not Live. No firmware changes were made. A patch-management flag can change when RAM patches are restored; sound bytes are verified separately and this is not a guarantee of identical global state. See the hardware report for exact differences.


## Local control API (no visible editor required)

The loaded processor exposes a private Unix socket under `/tmp/virus-ti-control-<uid>/`. The directory is mode 0700, sockets are 0600, and the server checks the connecting user's identity. It listens locally only; no HTTP port, network service, extra driver or bench host is needed. Creating the socket does not open the hardware. Each instance has its own PID/random socket name and removes it on destruction. Stale sockets from a crashed host are ignored by discovery.

From the project directory:

```sh
python3 tools/ti_control.py list
python3 tools/ti_control.py status
python3 tools/ti_control.py connect --wait
python3 tools/ti_control.py test-tone --wait
python3 tools/ti_control.py disconnect --wait
```

Use `--socket /tmp/virus-ti-control-501/<pid>-<instance>.sock` to target an exact instance returned by `list`. Without that option the client requires exactly one active/connected instance. The DAW must remain running with the instrument enabled; the editor may be closed, behind another window or off-screen. Inactive processors reject API connect/test-tone requests, including ordinary scanner instances. This API controls the plug-in's hardware service; creating tracks, launching DAW clips and recording still belong to the DAW. MIDI played through the DAW continues to follow the normal plug-in path.

Wire protocol version 1: send one ASCII command plus newline on one connection; receive one JSON object plus newline. Plain commands include `status`, `connect`, `disconnect`, `test-tone` and `banks`. JSON commands additionally provide bounded preset selection/import, project-state save/load, part-specific cutoff/note/routing commands and explicit emulator selection for headless tests; see examples below. The parser bounds request length and client lifetime. Mutations return a request ID; poll `status` until `completed_request_id` reaches it. Completion alone is not success: inspect `status` and `connected`, or use the client's `--wait` result and exit code. Concurrent hardware commands are rejected while one is pending. Editor buttons share the same command gate. Avoid multiple controllers issuing conflicting requests.

Status includes host activity/rate, connection state, snapshot directory, MIDI count, capture callbacks/errors, FIFO over/underruns, rendered frames, raw six-channel peaks and plug-in six-channel peaks. Counters and peaks are cumulative for that processor lifetime, so compare successive readings to establish current activity. Raw channels 3/4 correspond to plug-in USB1 channels 1/2. JSON snapshots are observations rather than atomic cross-thread measurements. All socket parsing, JSON and hardware commands run outside the audio callback; metering uses lock-free atomics.

There is no arbitrary MIDI, SysEx, file upload, firmware transfer or stored-bank write endpoint. `test-tone` uses only the existing stock RAM probe. `disconnect` uses the existing snapshot restoration path; if hardware is missing, restoration cannot be verified.

Validated in Live with the editor closed on 11 October 2026: discovery/status and a completed connection attempt returning the expected missing-device error. Native tests pass 4/4; Steinberg validation passes 47/47. Evidence: `analysis/daw-validation/api-validation.json`. After a TI power cycle, API connect/test-tone/disconnect/reconnect also passed in Live. A generated MIDI clip produced a 27.43-second stereo 24-bit 44.1 kHz recording with 20 detected note segments and a zero final half-second. Capture reported zero errors, overruns or underruns over 18,739 callbacks; all three edit buffers restored byte-for-byte. A separate no-I/O probe reopened the private interface after disconnect. The final instance is connected with the stock RAM test tone and the Live instrument track armed. No MCP registration is required for the CLI/API; an MCP adapter can call this same client later.


### Endurance-test finding

An eight-minute Live chord/wavetable sweep test completed six API reconnects with exact RAM restoration and zero reported capture/FIFO errors. All 48 scheduled rests were exactly silent, and steady-tone screening found no unexpected zero runs ≥2 ms or isolated discontinuity candidates. However, held-note disconnect hard-cuts the return to zero; the largest measured step was 0.04229 full scale, a possible audible click. A fade-before-disconnect remains to be implemented and tested. Evidence: `analysis/daw-validation/stress/summary.json`; procedure and limits: `docs/hardware-bench-validation.md`. No firmware changes.


### Preset and bank recall

The local browser now supplies the complete checksum-checked preset to the processor. Select any of the sixteen parts, then double-click a preset or press Return to recall it into that part’s TI RAM edit buffer. A selection made while disconnected is staged until Connect TI. Hardware readback updates the editor without emitting parameter automation.

Both bank menus include **Open bank or preset…** for local `.mid` banks and `.syx` single/bank files. Opening a bank adds it to this editor's library; selecting a sound recalls only that sound. Imported bank entries are session-local. No stored hardware bank writes or firmware operations are exposed.

The local API also supports these commands (zero-based IDs and indices):

```sh
python3 tools/ti_control.py banks
python3 tools/ti_control.py presets --bank 8
python3 tools/ti_control.py load --bank 8 --preset 0 --part 16 --wait
python3 tools/ti_control.py load-file --path /absolute/path/bank.mid --preset 12 --wait
python3 tools/ti_control.py load-file --path /absolute/path/preset.syx --wait
```

Reads and selections share the strict bank parser. Recalled dumps are addressed to bank 0, part 0–15 (API parts 1–16); Original Bank/Original Patch metadata is cleared to match TI RAM behavior and checksums are regenerated. The TI upgrades older 267/524-byte presets itself. The worker records the request and two matching readbacks, checks the name while preserving independent multi routing, and requires current-version sound bytes to match. A missed single-read reply is retried at most twice, with 200 ms spacing; the patch itself is not resent. Historical-format readbacks are reported as firmware-converted, not byte-exact source copies. This is preset conversion by the running stock firmware, not a firmware update.

Library recall targets all sixteen parts. Full project persistence is described below. Live browsing of stored hardware banks remains unfinished; imported banks are local files and loading an entry changes only its selected volatile part.


### Full sixteen-part project saving and recall

`Processor::getState` obtains two matching, checksum-validated RAM dumps for each of the sixteen parts and a validated multi dump. It includes edits made since preset loading and updates mapped controls for each part. The entire snapshot must succeed before replacing the cached project. Failed or unstable reads fail the save; playback remains connected on snapshot failure.

Version 3 retains the little-endian magic/version/count/ID/double prefix, followed by a 32-bit connection-intent flag and part count (16). Each of sixteen part records contains a 32-bit size (0, 267 or 524), bytes and CRC-32. A final multi record uses size 0 or 267 and the same length/bytes/CRC structure. Full TI states are 40,871 bytes. Virus checksums and CRCs must pass. Version 1 chunks and version 2 single-part chunks remain readable. All records are validated before changing parameters or staging recall, including errors in the final record.

`setState` stages all patches and the multi. A previously connected project reconnects when realtime processing begins, even with its editor closed. It restores multi routing and all sixteen volatile part buffers. An offline selection remains staged until Connect TI. Empty instances do not automatically open hardware. Only one instance may own the physical TI; unavailable hardware leaves the state staged for later connection. Bank files are not needed to restore the saved sounds; imported browser-bank entries remain session-local.

Save reads and recall execute outside `process()` and service MIDI while waiting. Sixteen-part snapshots can take substantially longer on physical hardware than the former Part 1 snapshot; a failed or changing readback does not silently substitute an older preset. Wait for recall completion before starting playback. Disconnect retains the plug-in’s sixteen sounds and multi separately while restoring the original external Single/part/multi RAM buffers. Explicit manual disconnect saves disconnected intent. This is sound and multi recall, not a snapshot of active voices, effects-tail memory or every global device setting.

The private API calls the **same actual VST3 processor callbacks** used for host component state:

```sh
python3 tools/ti_control.py state-save --path /absolute/path/sound.vtrstate
python3 tools/ti_control.py state-load --path /absolute/path/sound.vtrstate --wait
python3 tools/ti_control.py cutoff --value 37 --part 16 --wait
```

`state-save` completes synchronously after the hardware snapshot and writes atomically. `state-load` validates and stages; `--wait` waits for realtime recall, while `state_recall_pending` exposes deferred activation. API restore does not mark a DAW document dirty or invoke the host's project-save UI. Ordinary host saving uses `getState`; host reopening also calls the controller's `setComponentState`, which restores mapped controls and all sixteen editor patches.

Historical Part 1 hardware validation on 11 October 2026: four native suites and 47 Steinberg tests pass, including version 1 compatibility, every mapped-state truncation, full-patch truncations, CRC rejection, inactive staging and instance isolation. Live API tests captured post-load cutoff edits, switched presets, restored all sound bytes, rejected malformed states without changing the running sound, retained the patch across disconnect, and restored it with automatic connection in a fresh Live process. Six saves during a repeated-note clip each preserved the sound; 231 note-ons and 231 note-offs balanced, with zero USB capture errors or FIFO overruns/underruns. Snapshot-period MIDI gaps remained below 138 ms for messages scheduled 125 ms apart.

Evidence and installed executable hash: `../../analysis/daw-validation/project-state/summary.json`, `build-validation.json`, `roundtrip.json`, `restart.json`, `playing-validation.json` and `wire-audit.json`. All three audited sessions restored the original Single/part/multi dumps byte-for-byte; traffic contained no firmware command or stored-bank write. The TI normalizes patch-management flag byte 10 and checksums 265/522 on sound recall; all sound-data bytes matched.

**Validation limit:** Live's licence disables `.als` saving. At the user's direction validation used the component-state API, including a complete Live-process restart; an actual `.als` save/reopen is still untested. That historical hardware run covered Part 1. The sixteen-part build has emulator coverage and now passes physical sixteen-part recall and reconnect tests; see the 12 October hardware report below. An actual Live `.als` reopen still needs validation. Global hardware configuration, active voices, FX-tail memory and legacy vendor chunk compatibility are outside the saved-state contract.


### Testing without the TI connected

The optional OsTIrus backend runs the full TI synthesis engine through the same processor, control API, preset loader and state callbacks. It opens a local ROM file and does not enumerate/open the physical TI. Backend selection is explicit and **not embedded in project chunks**: configure `emulator` before restoring a project into a fresh test host. Emulator failure never falls back to hardware.

Build `osti-backend` in `native/osti-render` first, then configure/build the VST3 `VirusTIRework` and `vst3_api_host` targets. CMake copies the optional dylib into the bundle. The tested build is `build-multipart`; it has not replaced the older build already loaded in Live.

From the repository root, start the API host in a separate terminal (it runs for at most 30 minutes):

```sh
native/vst3/build-multipart/vst3_api_host /absolute/path/returns-six-channel.f32
python3 tools/ti_control.py list
```

Use the exact socket belonging to that host’s PID for every command, especially if Live is also running. Replace `<socket>` and `<firmware.bin>` below; the ROM is the outer installer firmware bundle, not raw `vti.bin`.

```sh
python3 tools/ti_control.py --socket '<socket>' emulator --rom '<firmware.bin>'
python3 tools/ti_control.py --socket '<socket>' connect --wait
python3 tools/ti_control.py --socket '<socket>' load-file --path /absolute/path/bank.syx --preset 15 --part 16 --wait
python3 tools/ti_control.py --socket '<socket>' route --part 16 --bus 3 --wait
python3 tools/ti_control.py --socket '<socket>' note-on --part 16 --note 60 --velocity 35 --wait
python3 tools/ti_control.py --socket '<socket>' note-off --part 16 --note 60 --wait
python3 tools/ti_control.py --socket '<socket>' state-save --path /absolute/path/multi.vtrstate
python3 tools/ti_control.py --socket '<socket>' state-load --path /absolute/path/multi.vtrstate --wait
python3 tools/ti_control.py --socket '<socket>' disconnect --wait
```

The headless host writes 48 kHz, six-channel interleaved float32 audio (USB1 L/R, USB2 L/R, USB3 L/R). It does not play to speakers. `tools/test_multipart_api.py` drives the full 16-part load/edit/route/audio/recall/reconnect test; `--restart` checks recall in a fresh host process. This validates sound generation and project data without hardware, but cannot validate physical USB drivers, USB dropouts, device-specific timing or restoration on the real TI. See `../../docs/multipart-emulator-validation.md` for measured results and remaining limits.

## Original appearance and live meters, 12 October 2026

Current hardware candidate: `build-original-switch/VST3/Release/Virus TI Rework Development.vst3`. The editor's **Modern UI / Original Access** switch is a local display preference. The original option reconstructs the supplied artwork, including the browser. The sixteen strips now show MIDI note-on activity separately from their editable CC7 faders; REA established this as the original meter behavior. Measured USB1–3 stereo capture peaks are shown separately in dBFS. Metering never enters sound state or automation.

API status adds `part_midi_activity` (16 normalized, decaying velocity peaks), `part_meter_source: midi-note-on-velocity`, and `usb_peak_meters` (three linear capture peaks in DAW bus order). Unlike `lifetime_*_channel_peaks`, these decay and clear on disconnect. The headless API host accepts optional `RATE BLOCK` arguments after its six-channel float output filename (default 48000 / 256), allowing physical resampler/block-size checks.

Physical USB3 requires the original Global USB controller 9 configuration. Hardware connection now selects value 1 (three synth returns, no plug-in input); teardown explicitly selects value 0 (standard two-return configuration), then sends Single mode. This is deliberate USB configuration normalization, not a readback-based restoration of unknown global settings. The original part/Single/multi sound snapshots are still restored independently. See [USB configuration evidence](../../docs/usb-three-return-evidence.md) and [current hardware validation](../../docs/hardware-matrix-2026-10-12.md). The emulator does not receive this hardware-only configuration change.


### Automatic restoration when the host closes

The `build-restoration` development build arms a bundled restoration helper before changing TI RAM. On plug-in termination or host exit, the helper restores and reads back the original sixteen parts, Single buffer and multi, sends Single mode and normalizes USB to the standard two-return setting. It keeps exclusive ownership until recovery finishes. A normal shutdown, an abruptly killed API host and explicit Disconnect have passed real-hardware checks; see [quit restoration validation](../../docs/quit-restoration-2026-10-12.md) for native Bitwig results and limitations.

Restoration takes approximately 23 seconds after host exit. Keep the TI powered and connected through that interval. Run `python3 tools/ti_control.py restorations` from the workspace root to read recovery status after the plug-in API endpoint has closed. No firmware or stored-bank writes are supported.
