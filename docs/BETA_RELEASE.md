# Virus TI Rework 0.2.0 beta 1

Prebuilt native Apple Silicon **VST3 and Audio Unit (AUv2)** plug-ins, with
Dark and Polar editors and a graphical setup app. No Access installer,
compiler or Terminal commands are required for normal operation. This is a beta;
tested stages and remaining checks are listed below.

1. Download **Virus-TI-Rework-0.2.0-beta.1-macOS-arm64.zip** from this release's
   Assets. GitHub's Source code archives are not the prebuilt plug-ins.
2. Unzip it in Finder and quit all DAWs/plug-in hosts.
3. Open **Virus TI Rework Setup.app**, select **Audio Unit** for Logic, **VST3**
   for Bitwig/Ableton, or both, and click **Install / Update**. LiveProfessor
   can use either format; this beta's recorded test used AU.
4. Restart the host, rescan if needed and load **Virus TI Rework** as an instrument.
5. Connect and power on the TI Desktop in Single mode. Click **Connect TI** and
   wait for initialization before sending notes on MIDI channel 1 for Part 1.
   Keep your normal DAW audio-output interface selected.

For manual installation, copy the complete `.vst3` bundle to
`~/Library/Audio/Plug-Ins/VST3/` and/or `.component` to
`~/Library/Audio/Plug-Ins/Components/` using Finder's Go to Folder. The AU works
without a separately installed VST3. Back up any existing bundle outside the scan
folders and move **Virus TI Rework Development.vst3** out before manual installation;
setup handles that backup automatically. Setup also offers uninstall while keeping
your patches, appearances and recovery data.

The [README setup guide](https://github.com/Give-me-a-signal/virus-ti-rework#setup)
contains the full installation, update and first-session instructions.

**This beta is ad-hoc signed, not Developer ID signed or notarized.** macOS may
block downloaded software. Use the per-app approval offered in System Settings
→ Privacy & Security where available. Do not disable Gatekeeper or SIP. Report
installation blockers with your exact macOS version.

Dark is the default on a fresh installation; Polar is included. For the optional
**Original Access** appearance:

1. Obtain your own **Virus TI Software Suite 5.1.7.00.pkg** from the
   [official Access download](https://www.virus.info/download/585) (Access sign-in
   may be required). Extract any outer archive in Finder; do not run the `.pkg`.
2. In Setup, click **Import Original Access Appearance…**, or choose
   **Import Original…** from the plug-in editor's appearance menu. Select the `.pkg`.
3. After import succeeds, choose **Original Access** in the editor. Select
   **Modern UI** to return to Dark/Polar.

Import is native, checksum-verified and shared between AU/VST3; it requires no
Python or developer tools and does not run the installer or vendor code. A failed
import leaves Dark/Polar usable. No Access artwork, factory patches, firmware or
executable code is bundled. See the [legacy appearance guide](https://github.com/Give-me-a-signal/virus-ti-rework#optional-original-access-appearance)
for the supported package and details.

This remains a beta: some controls are disabled pending hardware implementation,
MIDI timing is not sample accurate, and compatibility is specific to reported
setups. TI Desktop is the initial target; TI2/Snow are unverified. The packaged
beta has bounded Logic Pro, Bitwig, Ableton Live and LiveProfessor smoke tests
on M1 Pro/macOS 26.2; see `docs/daw-beta-validation-2026-10-15.md` for the tested
stages and outstanding work. GarageBand remains untested.

Use a disposable project and keep the TI powered and connected while restoration
finishes after disconnect or host exit. No firmware updates, stored-bank writes
or offline hardware rendering are supported.

Report successful and failed tests through the repository's Hardware / macOS
compatibility issue form, including Mac/chip, macOS, DAW/version, synth/firmware,
USB path, sample rate, buffer size and release version. See the included
manifest and the release's validator logs for build provenance.
