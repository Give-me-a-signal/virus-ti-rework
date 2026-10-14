# Automatic TI restoration on DAW exit — 2026-10-12

Implementation: the VST3 processor explicitly shuts down its hardware service on `terminate()`. Shutdown stops capture, closes the API, cancels pending snapshot/recall work and releases the USB handle. It does not wait for the approximately 23-second hardware restoration.

A bundled, signed helper starts for each hardware connection. Before the plug-in changes any RAM or mode settings, the helper validates and retains the original 16 part buffers, Single buffer and RAM multi. It inherits the same exclusive device-lock lease, and waits on a private pipe. Closing/unloading the plug-in, or the host process exiting, closes that pipe and triggers restoration. A short-lived launcher is reaped immediately; the independent helper is reaped by launchd, so no plug-in thread survives bundle unloading.

The helper checks the physical USB registry identity, stops held notes on every MIDI channel, restores the original RAM buffers and reads them back. It sends Single mode and normalizes USB configuration to the standard two-return setting. A failed operation does not prevent attempts to restore the other parts. Errors and readbacks remain in the connection's snapshot directory. Manual Disconnect continues to restore and verify in the plug-in, then disarms the helper.

Firmware and stored-bank writes remain blocked by the shared MIDI allowlist. This change does not flash firmware or write patch banks. It restores sound/routing data; it does not claim to capture unknown global settings. USB configuration and Single mode are the established exit normalization, not a readback of arbitrary previous globals.

Recovery progress remains readable after the DAW has exited:

```sh
python3 tools/ti_control.py restorations
```

Each session stores `restoration-status.json`, `restoration-helper.log`, and, for automatic recovery, `restoration-helper/restored-*.syx` plus captured private USB traffic. Keep the TI powered and connected until restoration reports `restored`. Device replacement/unplugging fails explicitly instead of writing snapshots to a different USB device.

## Validation

Evidence is in `analysis/quit-restoration-2026-10-12/`.

- Six CTest suites pass. Restoration regression coverage includes snapshot checksums and part identity, completing other parts after an injected failure, restoring an altered Single buffer, manual disarm, EOF handoff, and retaining exclusive ownership throughout recovery. The production helper also rejects corrupt snapshots before opening hardware.
- All 47 Steinberg VST3 validator tests pass.
- The installed bundle and nested helper pass deep, strict code-signature verification.
- Graceful API-host exit, with all 16 test sounds freshly read back and three notes held: host exited in 0.12 seconds, automatic recovery completed in 23.50 seconds. All 16 sound payloads, exact Single buffer and multi sound/routing matched the original snapshots.
- Abrupt API-host termination (`SIGKILL` of the test's own child process), under the same conditions: recovery completed in 23.44 seconds and all readbacks matched. This exercises the case where VST3 shutdown callbacks never run.

Single-part sound comparisons exclude the established firmware-normalized metadata/checksum bytes. Multi comparison excludes management flags and checksum. The standalone Single buffer is compared byte for byte. Captured recovery traffic is decoded and checked against the existing RAM/read/note/mode command allowlist.

Manual Disconnect also passed: its fresh project snapshot plus restore/readback took 39.26 seconds, all buffers matched, and the helper reported `completed-by-plugin` without performing a duplicate restore.

Native Bitwig 5.2.7 quit **passed** at 44.1 kHz / 256 host frames. The saved “Virus TI Hardware Recall 2026-10-12” project automatically recalled all sixteen sounds through the new build. Fresh API readback confirmed every sound and the multi routing; three API notes were left held with nonzero USB return meters. Sending normal Cmd-Q closed Bitwig without a Disconnect request. Its plug-in host exited, the helper restored the original RAM and verified every buffer in 23.48 seconds, and the exclusive device lease was released. Session: `45128321-8806-4DAB-9628-0B78904FE189`. The saved project hash remains `c0c4c5f4d03580667f7969f5a90523736cfcdbf350bb583d9b87ed80a55ef579`.

The previously required Disconnect-before-Quit workaround is superseded for this installed build. Bitwig is now closed and the original hardware RAM has been restored. The installed development symlink targets `native/vst3/build-restoration/`. Readbacks, status transitions, source-session copies, build hashes and MIDI audits are retained with `verification.json`.

The captured traffic audit covers 887 messages across the retained session and recovery captures. An abruptly killed host cannot finalize its own traffic capture; its helper's recovery capture is complete. Held notes were exercised and panic commands were captured, but this shutdown test does not claim a separate post-restore acoustic measurement.
