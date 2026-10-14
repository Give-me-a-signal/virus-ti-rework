# Physical TI validation, 12 October 2026

The complete 48 kHz hardware matrix, sixteen-part metering, 44.1/96 kHz checks and USB configuration normalization all pass. The physical matrix build was installed. Subsequent licensed Bitwig validation passed native project save/reopen with the revised build; see [Bitwig validation](bitwig-validation-2026-10-12.md), and the subsequently resolved [quit-time restoration](quit-restoration-2026-10-12.md). Raw reports are retained under `analysis/hardware-matrix-2026-10-12/`; a report whose `success` is false is not a passing matrix.

## Changes under test

- Original Access artwork switch, alongside Modern Dark and Polar, sharing controls and project state at 100–200% scale.
- Sixteen MIDI note-velocity activity indicators with independent CC7 volume faders. See [original binary evidence](part-meter-evidence.md). These are not individual audio dB meters.
- Three stereo capture peak meters, with independent, non-consuming API/UI reads and reset on disconnect. Their physical routing is being verified.
- CC7 replay after project recall and connection, so saved volume faders affect the TI.
- Silent output during initialization, followed by FIFO re-prime. Final-build initial connection produced all-zero host audio while preparing the TI (`final-startup.json`).
- Restoration checks allow only the TI-normalized multi management flag (offset 10) and its checksum (265); sound/routing bytes remain strictly checked. Both checksums and project CRCs remain validated.

## Completed software checks

Five CTest suites pass, including 64 CC7 drag checks (16 parts, both UI styles, two scales), independent activity/volume rendering, appearance-state preservation, USB meter mapping and transient-meter state exclusion. Steinberg VST3 validator: 47 passed, zero failed. Native preview dynamically loaded the final VST3, attached/edited/detached its view successfully. Both appearances were visually checked at 125%.

Logs: `final-ctest.log`, `final-validator.log`, `preview-selftest.log`. Final test host is the native `build-modern/vst3_api_host`, initially 48 kHz / 256 frames, recording six-channel float audio without speaker output.

## Hardware findings so far

All sixteen independent preset loads passed in the first hardware run, with the other fifteen sounds checked after every load. Later runs explicitly inherit and hash that evidence rather than claim it was repeated. Individual cutoffs and multi routing also read back correctly.

The initial idle threshold caught a small steady DC offset on the pair mapped as USB3. Its AC noise was much lower (about −98 dBFS and −112 dBFS RMS). The original failed report is retained; subsequent screening uses a recorded 0.0003 peak threshold.

The resumed three-bus matrix then found actual missing synth audio on USB3: parts 1 and 2 sounded on USB1 and USB2 respectively, but part 3 routed to USB3 returned only that low input-like signal. REA subsequently established the missing Global USB controller 9 command. The new candidate selects value 1 during connection and explicitly normalizes to value 0 on exit; its complete three-return playback/recall/reconnect matrix now passes. The earlier failing report remains evidence of the defect. The test restores initial RAM state and disconnects on failure.

## Scope and remaining checks

Edit buffers, MIDI controls, routing and USB I/O configuration are exercised. No firmware flashing or stored-bank writes are exposed by the API. Each hardware session saves its original sixteen part buffers, Single buffer and multi, and verifies restoration on disconnect. The early matrix report's scope string says “RAM edit buffers only”; the complete scope also includes host/Single mode and Global USB controller 9. USB configuration persistence is unproven and it is deliberately normalized on exit. This clarification does not change measured results.

This API host tests the actual processor/service but is not an Ableton `.als` save/reopen test. Live still reports saving/exporting deactivated because it is unauthorized. Its existing process also retains the older loaded plug-in. Wider DAW behavior, latency compensation, transport synchronization, and subjective click assessment remain separate from these signal/counter checks.

## Meter and volume hardware result

`meters-48000-two-bus/report.json` passed all sixteen independent MIDI activity/audio/decay tests on USB1/2, saved CC7=0 mute and audible-state recall, steady capture counters, and cleanup with restored RAM buffers. It intentionally excludes USB3 pending the separate routing fix. The original USB configuration has no verified readback; the new connection policy is documented as normalization, not exact global-state preservation. See [USB3 binary evidence](usb-three-return-evidence.md).

## Completed three-return matrix

`matrix-usb3/report.json` passes on the final USB3 candidate at 48 kHz / 256 host frames. It verifies all sixteen independent cutoff edits and routing readbacks, 48 isolated part auditions (sixteen before reconnect, sixteen after each of two reconnects), simultaneous three-note chords on all sixteen parts, full project recall after changing every cutoff and route, and atomic rejection of a bad multi CRC. Sound/routing readbacks and initial RAM restoration pass.

The three steady windows recorded 39,573 capture callbacks and 20,261,888 rendered frames, with zero capture errors, FIFO overruns or underruns. All audition samples were finite, none clipped, and release tails returned below the documented idle threshold. The 48-note chord peaked at about 0.0713 full scale. These are objective screening results, not a claim of exhaustive subjective click detection or sample-accurate MIDI timing.

The final three sessions' 1,635 transmitted MIDI messages were decoded from USB packet captures and passed `tools/audit_ti_midi_capture.py`. Evidence: `matrix-usb3-midi-audit.json`. The normalization command (Global USB controller 9 = 0) is explicitly included in that audit; it must not be described as a snapshot of the previous unknown global configuration.

Software validation for this final candidate: `usb3-ctest.log` (five suites), `usb3-validator.log` (47 tests), and `usb3-preview-selftest.log`. `usb3-tested-build-hashes.json` binds the final API host, VST3 and byte-identical preview copy. USB controller validation only permits configuration 0 or 1 and continues to reject other globals and firmware commands.

A normalized stereo listening mix of the sixteen initial auditions and combined chord is saved as `sixteen-parts-and-chords-preview.wav` (32.18 seconds). The raw six-channel capture and exact byte windows remain in the matrix report; `listening-preview.json` records the mix and normalization gain. Analysis measurements use the original unnormalized capture.

## Final rate, normalization and installation results

| Host sample rate / block | Physical check | Result |
| --- | --- | --- |
| 48 kHz / 256 | 16-part state/routing, 48 isolated auditions, 48-note chord, two reconnects | Pass |
| 48 kHz / 256 | All 16 MIDI activity indicators on USB1/2, saved CC7 mute and audible recall | Pass |
| 44.1 kHz / 64 | Parts 1–3 independently on USB1–3, meter response/decay, capture counters | Pass |
| 96 kHz / 512 | Parts 1–3 independently on USB1–3, meter response/decay, capture counters | Pass |
| Older 48 kHz host | USB3 returns to the original input-like signal after new teardown; the same patch sounds on USB1 | Pass |

Both rate checks finish with no added capture errors, overruns or underruns, finite/unclipped test audio, cleared meters and restored edit buffers. The TI's device stream remains at 48 kHz; the host-rate checks exercise the plug-in resampler rather than changing the hardware sample clock.

`two-return-normalization.json` verifies the exit policy with the unchanged older host, which never sends configuration controller 9. The same part/patch is silent when assigned to USB3 and audible when reassigned to USB1. This proves the prior two-return behavior has returned; it does not prove the value or persistence of every previous global setting.

`verification.json` indexes the five passing reports and limitations. `all-midi-audits.json` covers 5,722 messages across ten captured sessions, including the earlier failed diagnostic runs. No firmware or stored-bank command appears. Test hosts are disconnected and closed; the service verified the original sixteen edit buffers, Single buffer and multi sound/routing restoration.

The installed development VST3 symlink now targets `native/vst3/build-original-switch/VST3/Release/Virus TI Rework Development.vst3`. `installation.json` records its previous target for rollback. The offline preview embeds the identical tested plug-in. The open Live process still holds the old code; restarting would discard its unlicensed, unsavable Untitled set, so that final DAW check awaits the user's decision. An attempted offline snapshot of the old stale Live instance also failed (`live-offline-save.json`); no successful backup of that Live plug-in state is claimed.

## Subsequent Bitwig result

Licensed Bitwig 5.2.7 now passes native save/reopen, automatic reconnection, all sixteen sounds/multi and all 2,672 saved parameters, with 32 isolated auditions and zero USB/FIFO errors. The recall test used `build-bitwig`. Its discovered quit-time restoration failure is now fixed in the installed `build-restoration` build: [automatic restoration passes API and native Bitwig quit tests](quit-restoration-2026-10-12.md). Full evidence and boundaries are in [Bitwig validation](bitwig-validation-2026-10-12.md).
