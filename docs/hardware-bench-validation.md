## Preset/bank recall update (11 October 2026)

Complete Part 1 RAM preset recall is now connected to the browser and local API, including MIDI/SysEx bank files. The final real-hardware run passed 122 recalls across 61 populated banks, local file imports, and a five-pass Live recording of four distinct presets. USB counters stayed clean and final RAM restoration was exact. No firmware or stored-bank writes occurred. Full raw-patch DAW state persistence remains outside this change.

See [the preset validation report](../analysis/daw-validation/presets/README.md) and [recording](../analysis/daw-validation/presets/Live-preset-comparison.wav).

# Hardware bench validation — 11 October 2026

**The development VST3 now plays the TI Desktop in Ableton Live 12.3.5 and receives USB audio without the bench host.** Earlier bench evidence is retained below.

## Normal DAW validation

### Eight-minute endurance test: chords, sweeps and reconnects

The API-controlled endurance run completed 480 seconds with a repeating 60-second Live MIDI clip: held four- and eight-note chords, 32 rapid triad retriggers per loop, forward/reverse CC17 wavetable sweeps, five held-note positions, and scheduled rests. Six disconnect/reconnect/test-tone cycles occurred during held chords. Each cycle restored Single64, part0 and multi byte-for-byte and sent All Notes Off on all 16 channels (verified in private USB traffic). Reconnection plus stock-patch loading took 9.30–9.49 seconds. A final cleanup also restored all three buffers exactly; the plug-in was subsequently reconnected and left ready for keyboard playing.

Over the scheduled run, counters increased by 41,218 capture callbacks, 19,373,056 rendered frames and 37,910 MIDI messages, with zero capture errors, FIFO overruns or FIFO underruns. One-second polling found no capture stalls between deliberate reconnects. The 502.47-second stereo recording includes pre/post-roll; analysis aligns the scheduled 480 seconds at recording time 14.02 seconds using the first eight-second chord. The earlier pre-roll includes the short note fixture and must not be mistaken for stuck notes.

All **48 scheduled rest windows were exactly silent**. Waveform screening found no unexpected all-zero runs of at least 2 ms in the checked sustained passages and no isolated sample-discontinuity candidates within those passages. Position 0/32/64/96/127 produced median power-weighted spectral centroids of approximately 239/635/987/1335/1726 Hz, confirming audible-spectrum changes rather than merely logged CC messages.

**Finding: disconnect is not yet demonstrated click-free.** Capture cuts directly into silence during held notes. The first transition had a one-sample 0.04229 full-scale step (about −27.5 dBFS); another had a 0.01424 step. These discontinuities can produce clicks. The next implementation change should fade the captured return before stopping/releasing the stream, then retest held-note disconnects. These are deliberate disconnect-boundary cuts, not evidence of spontaneous USB packet loss. No fade fix was included in this test.

Evidence: `analysis/daw-validation/stress/summary.json`, `run.json`, `telemetry.jsonl`, `audio-analysis.json`, copied `sessions/`, `cleanup-wire-checks.json`, `final-disconnect.json`, `Live-stress-return.wav`, and `disconnect-1-original-level.wav`. Reproduce analysis with `python3 tools/analyze_daw_stress.py analysis/daw-validation/stress <original-live-recording.wav>`; `run_daw_stress.py` handles API cycles against the supplied MIDI fixture and schedule. Waveform screening is not an exhaustive listening test; the API does not expose USB packet timestamps, so no sample-level transport-continuity proof is claimed. Stock firmware and a volatile stock test patch only; no firmware flashing or stored-bank writes.


### Current API build: hardware pass after power cycle

The TI reappeared on USB after the user powered it off and on. Before new playback, the recovery helper restored the outstanding session's RAM state; Single64, part0 and multi all matched the original saved snapshots byte-for-byte (`power-cycle-recovery.json`).

The API then connected, verified the stock test patch, and captured hardware audio inside Live with its editor closed. Live's generated MIDI clip played notes 60/64/67/72, and its audio track recorded **27.434 seconds of stereo 24-bit 44.1 kHz USB1 return**. Peak 0.0309273, RMS 0.00775204; 20 audible note segments followed the test pattern, and the last half-second was exactly silent. Recording: `analysis/daw-validation/Ableton-live-api-build.wav`; measurements: `api-build-recording.json`. This uses stock hardware firmware and the stock HarmncSweep RAM probe, not modified firmware.

API disconnect completed successfully. Single64, part0 and multi each matched that session's before snapshots exactly. Counters were 18,739 capture callbacks, 8,811,520 rendered frames and 644 MIDI messages, with zero capture errors/FIFO overruns/FIFO underruns. A separate no-I/O private-interface probe opened and released interface 4 after disconnect, confirming ownership was released. See `api-disconnect-validation.json` and `api-interface-release/result.json`.

The API reconnected successfully, loaded the stock probe again and left Live stopped with the TI instrument track armed for keyboard input. The recorded clip is stopped and the audio track disarmed. These final settings and the active snapshot directory are recorded in `api-ready-status.json`. Capture counters are cumulative for the loaded processor and peaks are lifetime maxima; they do not imply signal during every sample. The fresh recording validates this build's note/audio path; historical silence and unplugged-device cleanup evidence below are retained.


The internal `HardwareService` owns capture and private MIDI on a control worker; no USB, file operations or blocking locks run in the processor callback. The user explicitly connects from the editor. Existing Apple audio and AURA MIDI drivers remain installed. A capture-only CoreAudio IOProc feeds a six-channel FIFO; raw channels 3/4 become plug-in USB1, 5/6 USB2 and 1/2 USB3. The TI stays at 48 kHz, while Live ran at 44.1 kHz. A windowed-sinc resampler and FIFO feedback accommodate the independent clocks.

Live recorded 1,058,400 stereo frames (24 seconds, 24-bit, 44.1 kHz), peak 0.751373 and RMS 0.084536. The user listened while playing and reported **“Sounds clean.”** The recording is preserved at `analysis/daw-validation/Ableton-live-TI-return.wav`; the original is in Live’s temporary recorded-samples folder. Live's licence state disables saving/exporting the set, but the recorded WAV exists and was copied successfully.

Across three connections of the first loaded service instance, cumulative counters reached 62,510 capture callbacks, 29,394,944 host frames and 6,796 MIDI messages with **zero capture errors, FIFO overruns or FIFO underruns**. These counters are cumulative for that instance and must not be summed between session logs. The first run's 24-second recording proves a nonzero DAW return; counters alone are not proof of continuous audible sound or sample-accurate MIDI. See `analysis/daw-validation/validation.json` and its copied `sessions/` directory.

### Restoration correction

After the first DAW session, Single64 matched exactly; part 1 and multi sound/routing were restored, but bank/program references were 2/3 instead of 0/0. Restoring the multi RAM dump alone does not restore those references. The corrected sequence restores multi RAM, explicitly sends the saved page-0x72 part-1 bank/program values (indices 32/33), then restores part-1 RAM. A separate private-MIDI process successfully reopened the interface after disconnect and verified this sequence against the initial DAW snapshots: Single64 and multi match byte-for-byte; part-1 sound bytes and bank/program references match. Part-1 patch-management flag at full SysEx offset 10 changed 0→5, with checksum offsets 265/522 updated. Its meaning remains unresolved; full-state identity is not claimed.

The corrected service also drains worker autorelease pools regularly, releases its interface explicitly and suppresses hardware notes/CCs during offline processing. Current release tests pass (processor, native editor and 44.1/48/96 kHz FIFO simulation); Steinberg validator reports 47 passes, zero failures. The final bundle is locally ad-hoc signed. The separately built `native/vst3/build-daw-final/` is the target of the user's VST3 symlink. **Reload confirmed:** after the approved restart, Live PID 92693 maps the corrected `build-daw-final` binary. Session `283699A5-B9B3-49B7-A6C8-3D22C7B8FC32` logs private note-on/off delivery from both user playing and the generated MIDI clip. The current Single patch produced two silent recordings (`reload-initial-recordings.json`). The stock test-tone control subsequently passed its full patch readback, but another 21.49-second stereo recording was silent (peak/RMS zero; `corrected-build-recording.json`). On the subsequent reload, macOS no longer enumerated the TI USB device or public MIDI endpoint. The old session logged invalid audio-device/private-USB errors during cleanup and produced no restored snapshots (`corrected-build-restoration.json`). Restoration and the cause of the earlier silence remain unresolved; device absence at cleanup does not prove it caused every earlier silent recording. No successful audio retest is claimed yet. Deleting and reinserting the device before restarting had retained the original mapped binary.

**Local API follow-up:** the installed symlink now targets `build-api`. Discovery, status and an asynchronous connect failure were verified from Live PID 16335 with the editor closed. The API reports raw/rendered peaks and capture counters; it exposes no firmware or arbitrary SysEx endpoint. New native tests pass 4/4 and the VST3 validator passes 47/47. That initial missing-device result was subsequently superseded by the hardware pass above. See `analysis/daw-validation/api-validation.json`.

Use instructions and development limitations: `native/vst3/README.md`. No firmware image was transferred, no stored patch bank was written, and no driver was installed or replaced.

## Private MIDI hardware pass

The new `native/private-midi/private_usb.hpp` opens only the unclaimed second vendor interface (interface4, interrupt OUT06/IN86, max32 bytes), without seizing the device or taking over the existing audio/public MIDI interfaces. Read requests and temporary control changes travel over the observed private route2; replies currently arrive over AURA/CoreMIDI. No hidden-IN payload was observed, so private bidirectional communication is not claimed.

REA traced original host events through `50aa6 → ab030 → edfbe → eacc8`, confirming route2 and optional F5 timestamp behavior. Sources: `analysis/rea-private-host-events-evidence.json`, `analysis/private-host-events-rea.c`; host-event evidence `ev_4fc616ce9c111c64a66dd09cff2ad826e7b24193f4fe612c84da415f12790910`. The original audio-start trace did not reveal an additional Virus-specific activation command: `analysis/rea-private-audio-activation-evidence.json`.

The original part0 patch remained silent. A known stock HarmncSweep patch, loaded only into its RAM edit buffer at reduced patch volume45, produced sound. This establishes that the transport and audio route work; the precise reason the earlier patch was silent remains unresolved. The probe uses stock hardware firmware and the stock wavetable, not the experimental replacement.

Final evidence: `analysis/hardware/private-notes-final/result.json`, `note-validation.json`, `private-out.bin`, `raw-usb.wav`, `vst3-returns.wav` and the stereo audition `hardware-three-notes.wav`. Recheck offline with `python3 tools/analyze_hardware_notes.py analysis/hardware/private-notes-final`.

| Check | Final observation |
|---|---|
| Actual VST3 note events | MIDI60/64/68, velocity36, with note-offs; route2 wire words verified |
| Pitch | 261.861, 329.910, 415.649 Hz; all within1.6 cents of equal-tempered targets |
| USB audio | 384,000 frames at48 kHz; 750 callbacks; zero timestamp gaps, callback errors, publish failures, underruns or dropped MIDI |
| Return order | Raw channels3/4 feed VST3 bus1,5/6 bus2,1/2 bus3; exact sample equality after permutation |
| Note-off | Final synth return exactly silent from6.3 seconds; intermediate rests within2 native16-bit LSB |
| Final-run restoration | Part0, Single64 and multi RAM dumps byte-identical to that run's snapshots |
| Mode | Single-mode restore command sent over the working private path; patch replies continue |

The first stock-patch load upgraded its preset version8→12. On restoration, the original part0 sound parameters returned, but full-dump byte10 (`Single Patchmanagement/Flags`) changed1→0, with both checksums updated. Its bit0 meaning remains unresolved. Single64 and multi remained exact. Subsequent note runs restored all three dumps byte-for-byte against their own pre-run snapshots; this does **not** erase the earlier flag difference. The earlier public-mode test also left the separately recorded multi header difference below. No complete global snapshot exists.

The host verifies backups were saved before mutations and attempts all note-offs and restorations even if one operation fails. It keeps separate SysEx assembly for each public MIDI source. This remains a prototype: MIDI is sent after completed VST3 blocks, not sample-accurately; an underlying USB abort failure can leave a request waiting indefinitely. At that bench stage, unplug recovery, nonzero computer-to-TI playback, long soak, other sample rates, other sounding return buses and ordinary DAW integration were not validated. The normal DAW section above updates the latter two host-integration/sample-rate findings.

## Earlier public-MIDI bench results

The sections below preserve the initial failed-note evidence and its restoration history; the private-interface pass above supersedes their note/transport status.

No firmware updater, firmware image transfer, stored patch-bank write, driver installation, USB reset, or takeover of an existing audio/public MIDI interface was performed. The experimental wavetable firmware was not used on hardware.

## Observed results

| Check | Result | Evidence |
|---|---|---|
| USB discovery | Virus TI, VID 0x133e/PID 0x0815 | `analysis/connected-probe.json` |
| Existing audio driver | Apple `usbaudiod`; 6 input and 2 output channels, float32, 48 kHz | `analysis/hardware/query/result.json` |
| Existing MIDI driver | AURA Plugins Virus TI USB Midi; External and Plugin I/O online | Discovery probe and installed-binary REA |
| Live audio into actual VST3 | Five 8-second captures; no reported timestamp gaps, queue failures, callback errors or bridge underruns | `analysis/hardware/validation-summary.json` |
| Three VST3 stereo return buses | All six channels exactly match raw CoreAudio samples | Per-run `raw-usb.wav`, `vst3-returns.wav`, `result.json` |
| MIDI SysEx request/reply | Valid checksums for Single, part 1 and multi edit buffers | `analysis/hardware/state-snapshot/` |
| Ordinary VST3 CC40 | CoreMIDI accepted it; neither queried patch changed | `analysis/hardware/vst3-cc-readback/` |
| VST3 automation through Single SysEx adapter | Cutoff 21 → 32 on hardware, then restored to 21; both single dumps restored exactly | `analysis/hardware/vst3-sysex-parameter-readback/` |
| VST3 note-on/off | Six events delivered to CoreMIDI without bridge drops; no corresponding synth signal detected in USB capture | `analysis/hardware/notes-current-patch/` |
| Plug-in validation | Native processor tests pass; Steinberg validator 47 passed, 0 failed | `native/vst3/build/validator-bridge-interface.log` |

The tiny signal on USB channels 1/2 resembles analogue-input noise; channels 3–6 are zero in these captures. Its origin is inferred, not proven. Streaming silence/noise successfully is not proof of synth routing or meaningful signals on all three return pairs. USB playback buffers were deliberately zero-filled; nonzero playback, loopback latency, 44.1 kHz, long-duration stability and a normal DAW/editor session remain untested.

## Mode test and restoration

The user confirmed initial Single mode. The shipped XML maps page 0x73/index 0x7A/value 3 to the original plug-in's initialization mode, and page 0x72/part 0/index 0x29/value 10 to USB1 stereo. A bounded test saved both relevant singles and the multi before attempting those settings. The panel changed to SEQ; patch replies then stopped. The test aborted before audio started. Its automatic restore commands and the original plug-in's standalone cleanup commands did not produce confirmation.

The user pressed Single. Replies resumed. Both single edit buffers are byte-identical to their pre-test snapshots. The mode transition also changed multi part 1 volume from 85 to 64; the saved multi was retransmitted **to its RAM edit buffer only**, restoring all XML-mapped parameter bytes including volume and output routing. One undocumented header byte at SysEx offset 10 remains 5 instead of 3, with the corresponding checksum change. Its meaning is unresolved; do not describe the whole multi or all global state as byte-identically restored. No global snapshot was available. The user confirmed the final panel mode is Single.

The `--plugin-notes-from-single` experiment is quarantined in the bench executable. Do not repeat mode entry or assume the original plug-in's broad initialization sequence is reversible.

## REA transport finding

The installed AURA driver creates External entity 0 and Plugin entity 1. It uses the entity index as USB-MIDI cable number, so an ordinary CC from Plugin I/O is encoded as `[1B B0 28 value]`. The original Virus Control private encoder uses additional routes 2/3/5 and a timing marker `[53 F5 low high]`. AURA's serializer explicitly discards F5 and subsequent data until the next status; passing raw F5 through CoreMIDI cannot construct the private marker.

This is concrete evidence that the current endpoint cannot reproduce the original private transport. It is **not a proven explanation for every hardware symptom**: the exact reason for the SEQ communication loss and the inactive ordinary CC remains unresolved. MIDI channel and Low Page configuration can also matter.

Sources: `analysis/rea-aura-midi-routing-evidence.json`, `analysis/aura-midi-routing-rea.c`, `analysis/aura-midi-output-rea.asm`. Serializer evidence `ev_30837fafede7252a81a6a2a30bb4c3cc58853ff9d6bcff347392c05e0e4c0f6c`, assembly `ev_725fda3316a8fb07a64070f701da55eaee2fbb60934c5693614b25a590badb50`. Installed universal binary SHA-256 `c5f113347de2271d48480034cb1cba6c3914e1726d8200a386d2856ac0c97f75`; analyzed arm64 slice `013e352ae70bfbce4d4e6c2310d0fa235a45a5d915a1eec89fb8ccffec10b2e2`.

Original initialization and cleanup: `analysis/rea-hardware-routing-evidence.json`, `analysis/hardware-routing-init-rea.c`; init evidence `ev_963efc8cf85a1c9b86112b295a2cf41d97b305cb00b3957ce28b2208ef820f66`, cleanup `ev_4edf26b44d34d32b4b227b6334a0bcc0bbeb38c4769077a38e6f3312fccb6eeb`. The original cleanup sets standalone defaults rather than restoring a prior global snapshot.

## Initial implementation boundary (subsequently crossed above)

`native/hardware-bench` loads the built `.vst3`, queries its new private in-process bridge interface, activates all three stereo buses, and runs the processor from the TI's CoreAudio capture callback. The callback supplies the plug-in sample clock directly at the observed matching rate. The control thread drains bounded MIDI events after each processed block. MIDI delivery is immediate after block completion, not sample-accurate scheduling.

The Single SysEx adapter translates only this test's VST3 part-1 cutoff CC into the XML-backed Single edit-buffer parameter message. It is a scoped bench adapter, not a completed 16-part parameter service. Restored patch dumps and hashes make the test reviewable.

The next implementation boundary is a transport capable of the TI private control routes, with an understood mode-entry/exit handshake and state restoration. Preserve the working Apple audio path while investigating that boundary. Firmware flashing is not required for this driver work and remains prohibited by the user.
