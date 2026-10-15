# Virus TI Rework — unverified Snow playback testing

Snow is **unverified on physical hardware**. Current PR #9 builds allow the
regular **Connect TI** action to select Snow's four-part path; a special compile
flag or second test button is no longer required. The test-build flag controls
packaging/branding only. Saved Snow projects still require a manual connection;
scanning, opening the editor and parsing state do not connect.

Published downloads are immutable: **0.2.0-beta.1 remains unchanged**, and the
earlier **0.2.0-snow-test.1** download still requires its **Snow test…** button and
confirmation. The connection change is in the PR, not those existing releases.
Firmware, stored-bank writing and USB resets remain blocked. No emulator,
firmware, factory sounds or Access assets are included.

## Install

Quit your DAWs. Unzip the download and open **Virus TI Rework Setup.app**.
Select VST3 for Ableton Live/Bitwig, or AU for Logic, then click Install / Update.
Restart the DAW and rescan if necessary. Setup preserves previous installations
in your user's Application Support/VirusTIRework/PluginBackups directory.

Manual copy: put Virus TI Rework.vst3 in ~/Library/Audio/Plug-Ins/VST3, or
Virus TI Rework.component in ~/Library/Audio/Plug-Ins/Components. Back up existing
copies first and remove the old Development bundle from the scan folder. The AU
is independent; it includes its own engine and recovery helper. These builds use
the existing IDs and replace your installed Rework, so use a disposable project.

Builds have ad-hoc signatures, not Developer ID signatures or notarization.
If macOS blocks the downloaded app, inspect the release, then use the individual
Open Anyway approval in System Settings → Privacy & Security if offered.
Do not disable SIP/Gatekeeper or run blanket quarantine-removal commands.

No Access installer is needed for Dark/Polar. Optional Original Access artwork
still uses the normal importer and the owner's own supported installer.

## Before starting

- Use a disposable project and preserve important sounds with your normal backup
  method. Close other Virus clients. Put the Snow in **Single mode** and keep it
  powered/connected until restoration completes. Start with low monitoring volume.
- macOS must expose compatible **CoreMIDI Plugin I/O ports**, as well as USB audio.
  This package does not include a replacement MIDI driver. If MIDI Studio has no
  Virus ports, connection will stop with a missing-port message; report the driver
  name/version or “none/unknown”. Do not run the old Intel installer to force a pass.
- This first preview requires the observed 0x133e:0x0815 USB transport profile and
  a Snow product name. Product 0x0816 remains blocked pending descriptor validation.
- Original Single/part buffers must use current format (12 or later); part
  original-bank metadata must be canonical. Legacy or unvalidated originals are
  rejected **before RAM edits**, because exact restoration is not established.

## Playback test

1. Load the current PR build in a fresh instrument track. Put Snow in Single
   mode, then click **Connect TI**. The detected Snow selects four active parts
   and displays **Snow hardware unverified**. No extra opt-in button is required.
   If testing the earlier published snow-test.1 ZIP, use **Snow test…** instead.
   Neither path connects during scanning or editor opening.
2. Wait for initialization. Before edits, the test reads all four parts plus
   Single/Multi twice, checks identities/checksums/stability, saves private recovery
   data and arms the helper. A failure stops the connection; do not bypass it.
3. Play a short MIDI clip on channel 1, then channels 2–4 separately. Check note
   releases and stuck notes. Parts 5–16 are unavailable. Test USB1/2 only: the host
   retains three buses for project compatibility, but Snow bus 3 is silent.
4. Save the disposable project. Disconnect and wait for the restoration result.
   Reopen and click **Connect TI** again to check sound/routing recall (use
   **Snow test…** for the earlier snow-test.1 download).
5. Separately, quit the host normally while connected. Keep the Snow powered and
   connected until the independent helper finishes. Do not deliberately force-quit,
   unplug or power-cycle as a routine test.

Stop on identity, checksum, snapshot or restoration errors. Keep recovery files
and report the exact message; do not repeatedly retry writes. Original buffers,
recordings and personal projects stay private. “Firmware writes blocked” is not
a guarantee that every model-specific RAM/mode behavior is correct.

## Report

Reply in issue #8 with Mac/chip, macOS version/build, Snow firmware, DAW/version,
native/Rosetta mode, AU/VST3, USB path, MIDI driver/port names, rate/block and the
exact stage/result. Distinguish no audio, stuck notes, save/reopen and restoration.
Crop screenshots and remove serial numbers, location IDs and personal paths.
Do not upload raw patches, full system reports or entire recovery directories.

Local Snow-firmware emulation exercised four-part audio, repeated recall and
current-format restoration. The emulator models the microcontroller/MIDI layer;
it does **not** validate physical USB replies, drivers, channel order or recovery.
Real Snow testing and TI Desktop regression after these changes remain outstanding.

Project: https://github.com/Give-me-a-signal/virus-ti-rework/pull/9
Feedback: https://github.com/Give-me-a-signal/virus-ti-rework/issues/8
