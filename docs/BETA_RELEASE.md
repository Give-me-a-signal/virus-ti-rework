# Virus TI Rework 0.2.0 beta 1

Prebuilt native Apple Silicon **VST3 and Audio Unit (AUv2)** plug-ins, with
Dark and Polar editors and a graphical setup app. No Access installer,
compiler or Terminal commands are required for normal operation.

Download the macOS arm64 ZIP, unzip it, quit your DAW and open **Virus TI Rework
Setup.app**. Choose your formats and click **Install / Update**. Alternatively,
copy the included `.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `.component`
to `~/Library/Audio/Plug-Ins/Components/`. Restart your DAW and rescan.
Remove the previous Development plug-in from the scan folder before manual
installation; setup backs it up automatically.

**This beta is ad-hoc signed, not Developer ID signed or notarized.** macOS may
block downloaded software. Use the per-app approval offered in System Settings
→ Privacy & Security where available. Do not disable Gatekeeper or SIP. Report
installation blockers with your exact macOS version.

Dark is the default. For the optional **Original Access** appearance, select
**Import Original Access Appearance…** and choose your original, supported
5.1.7.00 Access package. It is extracted without running its installer or code.
No Access artwork, factory patches, firmware or executable code is bundled.

This remains a beta: some controls are disabled pending hardware implementation,
MIDI timing is not sample accurate, and compatibility is specific to reported
setups. TI Desktop is the initial target; TI2/Snow are unverified. AU validation
and our test host do not establish Logic/GarageBand compatibility.

Use a disposable project and keep the TI powered and connected while restoration
finishes after disconnect or host exit. No firmware updates, stored-bank writes
or offline hardware rendering are supported.

Report successful and failed tests through the repository's Hardware / macOS
compatibility issue form, including Mac/chip, macOS, DAW/version, synth/firmware,
USB path, sample rate, buffer size and release version. See the included
manifest and the release's validator logs for build provenance.
