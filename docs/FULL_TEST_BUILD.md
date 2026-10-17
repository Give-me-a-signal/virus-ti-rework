# Virus TI Rework 0.2.0-test.1 — full tester build 2005

This Apple Silicon AU/VST3 prerelease combines ownership/recall hardening (#6),
the timing implementation (#7), model-aware Snow connection/recovery (#9), and
the restored Dark/Polar layout and editor refresh fixes (#11). It is a separate
integration candidate, not a replacement for the published beta or a claim that
the four PRs are reviewed and ready to merge.

## Known limits — read before testing

- **Connected Logic AU save/reopen remains unresolved.** Previous testing
  retained earlier project state despite successful native snapshots. Do not
  trust a project save to preserve a newly edited sound until you have compared
  a reopened disposable project. Synthetic AU state round trips do not settle
  this host issue. Report failures rather than relying on the project for work.
- **Synchronized timing is disabled by default.** The earlier opt-in AU timing
  matrix failed at 48 kHz/512 frames with a missing onset/FIFO underruns. Those
  results are not reclassified as passing by this build. Use ordinary immediate
  playback; do not set the experimental marker-timing environment option for
  general testing. Snow marker calibration/timed writes remain disabled.
- **Physical Snow support is unverified.** Normal Connect TI selects four
  parts and USB1/USB2; parts 5–16 and USB3 are unavailable. Originals must be
  current-format, canonical and stable across two reads before any RAM writes.
  Connection can refuse legacy/unsupported originals; do not bypass that check.
  Snow project reopening requires manual connection. Emulation is not evidence
  of real USB/driver compatibility or physical RAM restoration.
- Earlier physical results cover TI Desktop on M1 Pro/macOS 26.2. **This
  integrated revision has not been tested on physical hardware.** TI2, Polar,
  keyboard variants and other Mac/OS combinations require model-specific reports.
- Full parameter editing is unfinished. Dimmed controls are read-only; graphs
  are illustrative. The refresh change reduces proven unnecessary painting,
  but the reported Logic freezes have not been reproduced or declared fixed.

## Download and install

Download `Virus-TI-Rework-0.2.0-test.1-macOS-arm64.zip` from the GitHub prerelease,
not either Source code archive. Check the adjacent SHA256SUMS.txt if desired.
Unzip, quit all DAWs, open **Virus TI Rework Setup.app**, select AU and/or VST3,
and click **Install / Update**. Reopen your DAW and select **Virus TI Rework**.

Setup installs into your user account and preserves previous bundles under
`~/Library/Application Support/VirusTIRework/PluginBackups`. It migrates the old
Development VST3. The test and beta have identical plug-in IDs; keep one copy per
format, including across the user/global plug-in folders. Restore a previous
version by quitting the DAW, using Setup to uninstall the test, and reinstalling
the previous release. Imported patches, appearances and recovery data are kept.

For manual installation in Finder → Go → Go to Folder:

- `.component` → `~/Library/Audio/Plug-Ins/Components/`
- `.vst3` → `~/Library/Audio/Plug-Ins/VST3/`

The AU embeds the complete VST3 and works on its own. Use a native Apple Silicon
host. Intel-only Logic 10.4.8 cannot load this arm64-only AU. The macOS deployment
target is 12; that is not a claim that every supported OS/DAW has been tested.

These bundles are ad-hoc signed, not Developer ID signed or notarized. If macOS
blocks Setup, use its per-app **Open Anyway** approval in System Settings →
Privacy & Security when offered. Do not disable Gatekeeper/SIP or remove
quarantine from broad folders. Report any unresolved approval or scan block.
No clean-account browser-download approval test has been completed for this ZIP.

Dark and Polar work without Access content. To use the optional Original Access
appearance, choose **Import Original Access Appearance…** in Setup, or
**Import Original…** in the editor. Select your own supported Virus TI Software
Suite 5.1.7.00 package from [Access](https://www.virus.info/download/585). The import
checks/extracts it without running its installer. Do not run the legacy package.
No Access artwork, factory banks, emulator runtime or firmware is bundled.

Hardware operation still needs compatible existing CoreAudio/CoreMIDI drivers;
this package installs no hardware driver. Allow the DAW microphone access if
macOS requests it. A working audio device and Plugin I/O MIDI ports are required.

## First tests to report

1. Use a disposable project. Record your Mac/chip, macOS/build, DAW/version,
   format, synth model/firmware, driver/version, USB hub/adapter path, sample rate
   and buffer size. Quote build 2005 and the commit in `manifest.json`.
2. Check scanning and editor opening with the synth disconnected. Browse each
   page in Dark/Polar; check knobs, menus, zoom, resizing and idle responsiveness.
3. With the synth powered/connected, use Connect TI. Report initialization
   errors verbatim with identifiers and private patch names removed. Keep
   monitor volume low during connection tests. Stop on identity/checksum errors.
4. Play short notes, verify note-offs and the available USB returns. Check
   supported automation and compare editor-open versus editor-closed playback.
   Change one supported sound control reversibly in a RAM edit buffer only.
5. Save/reopen the disposable project and compare sound, parameters and routing.
   Treat connected AU recall as a diagnostic test, not a reliable workflow.
6. Disconnect normally, then test normal host exit. Keep the device powered and
   connected until restoration reports completion. Report any restoration error;
   do not reset or power-cycle the device to force the test to pass.

No firmware updates, stored-bank writes, automatic bank synchronization or USB
resets are supported. Testing writes volatile edit buffers and attempts to
restore them; physical compatibility is not guaranteed by the command boundary.

Post successful, failed or partial results in a
[GitHub issue](https://github.com/Give-me-a-signal/virus-ti-rework/issues), including
reproduction steps, expected/actual behavior, format and build. For lag/freezing,
include theme/page/zoom, connection status, whether it happens while idle or
editing, and whether closing the editor helps. Share redacted logs only; raw
patch dumps, personal projects, recordings and device identifiers stay private.
Ableton save/reopen remains untested here because authorization was declined;
LiveProfessor remains deferred. Tester reports are welcome for both.

## Reproduce and inspect

From a clean checkout with the pinned SDKs, run:

```sh
VTR_FULL_TEST_BUILD=1 VTR_BUILD_DIR="$PWD/native/vst3/build-full-test" bash tools/build_beta.sh
```

No Access package or resource cache is required. The full-test packager verifies
the build flag, rejects experimental timing-DAW builds, audits arm64/system
dependencies and vendor/private payloads, and includes signatures, checksums,
dependency notices and `release-validation.json`. GitHub CI also validates the AU.
The manifest links the exact source commit; see the integration PR for its checks.
