# Virus TI Rework

**Native Apple silicon AU and VST3 integration for the Access Virus TI hardware synthesizer.**

An independent, open-source community project by **Give Me a Signal** to keep owned Virus TI hardware useful on modern Macs. The hardware makes the sound; this plug-in brings its MIDI control, USB audio returns, patch browsing and project recall into a native DAW.

**Status: 0.2.0-beta.1 — native AUv2 and VST3 beta.** The packaged plug-ins have passed short real-hardware audio checks in Logic Pro, Bitwig, Ableton Live and LiveProfessor on an M1 Pro running macOS 26.2 with a TI Desktop. Coverage differs by host; see [tested setups and outstanding checks](#tested-setups). This remains a beta with incomplete Virus Control features and limited hardware coverage. Builds are ad-hoc signed and are not Developer ID signed or notarized. New project code is MIT licensed.

**Normal installation needs no compiler, Terminal commands or original Access installer.** Dark and Polar are included. Importing your own Access installer is optional and adds the legacy appearance.

![Virus TI Rework — Modern Dark filter page](docs/screenshots/dark-page-03.png)

[Install](#setup) · [Legacy appearance](#optional-original-access-appearance) · [Tested setups](#tested-setups) · [AI-assisted setup](#ai-assisted-setup-for-non-technical-users) · [What works](#what-works) · [How it works](#how-it-works) · [Engineering history](#how-we-built-it) · [Screenshots](#interface-gallery) · [Limitations](#known-limitations) · [Help test](#help-test-on-more-macs-and-synths) · [Contribute](CONTRIBUTING.md) · [Release packaging](#packaging-and-binary-releases)

## Setup

### 1. Download the prebuilt plug-ins

Open [GitHub Releases](https://github.com/Give-me-a-signal/virus-ti-rework/releases), expand **Assets** on
the published beta, and download **`Virus-TI-Rework-0.2.0-beta.1-macOS-arm64.zip`** (or the equivalent
ZIP for a newer release). Choose the named macOS arm64 ZIP; GitHub's **Source code** archives contain
source files, not ready-to-install plug-ins. If no release has an attached macOS ZIP yet, the binary
release has not been published.

Double-click the ZIP in Finder to extract it. The download includes:

| File | Purpose |
| --- | --- |
| **Virus TI Rework Setup.app** | Graphical installation, updates, uninstall and optional legacy appearance import |
| **Virus TI Rework.vst3** | Prebuilt Apple Silicon VST3 instrument |
| **Virus TI Rework.component** | Prebuilt Apple Silicon Audio Unit (AUv2) instrument |
| **INSTALL.txt** and validation/dependency notices | Installation notes, build provenance and dependency licences |

### 2. Choose your plug-in format

| DAW | Format to install | How to load it |
| --- | --- | --- |
| **Logic Pro** | **Audio Unit** (`.component`) | Create a software-instrument track and choose Virus TI Rework from the AU instrument menu |
| **Bitwig Studio** | **VST3** (`.vst3`) | Search the plug-in browser for Virus TI Rework and add it to an instrument track |
| **Ableton Live** | **VST3** (`.vst3`) | Enable VST3 scanning if needed, then load Virus TI Rework from Plug-Ins on a MIDI track |
| **LiveProfessor** | **Audio Unit** or **VST3** | Rescan in Plugin Manager, then add Virus TI Rework; this beta's recorded LiveProfessor test used AU |
| **Other native Apple Silicon hosts** | A format supported by your host | Rescan its plug-in folders and look for Virus TI Rework; compatibility needs a separate test report |

The AU contains its own engine and restoration helper. **Installing the VST3 separately is not required
to use the AU.** You can install both formats for use in different hosts, but only one instance may
connect to the TI at a time.

### 3. Install with the setup app

1. Quit all DAWs and plug-in hosts before installing or updating.
2. Open **Virus TI Rework Setup.app** from the extracted download.
3. Select **VST3**, **Audio Unit**, or both, then click **Install / Update**.
4. Wait for the app to report success. It installs into your current user's plug-in folders; it does
   not require a system-wide installation.
5. Reopen your DAW, rescan if necessary, and load **Virus TI Rework** using the format guide above.
6. Follow [your first hardware session](#first-hardware-session) below to connect the synth and play.

**Dark is the default** on a fresh installation. Use the editor's theme button to switch to **Polar**.
Unfinished hardware controls are dimmed and disabled. Factory sounds are not bundled: use captured
hardware edit buffers or import your own `.mid`/`.syx` patches through the browser's bank menu.

### If macOS blocks the download

These are **ad-hoc signed, unnotarized beta builds**. If macOS blocks Setup after you try to open it,
check **System Settings → Privacy & Security** for a per-app **Open Anyway** option. Use it only if
you trust the release you downloaded, then confirm the specific app when prompted. See
[Apple's instructions for opening downloaded apps](https://support.apple.com/en-gb/102445).

Approval options vary by macOS version, and a DAW scan may report a separate plug-in block. If the
relevant approval is unavailable or scanning still fails, report the exact alert and macOS version.
A clean-user, browser-downloaded installation has not yet been validated. Do not disable Gatekeeper
or SIP or remove quarantine flags across your plug-in folders. See also the ZIP's **INSTALL.txt**.

### Manual installation

Prefer to copy the plug-ins yourself? Quit all hosts, then use Finder's **Go → Go to Folder…**
(`⇧⌘G`) to open the appropriate destination:

| Copy this complete bundle | Into this folder |
| --- | --- |
| **Virus TI Rework.vst3** | `~/Library/Audio/Plug-Ins/VST3/` |
| **Virus TI Rework.component** | `~/Library/Audio/Plug-Ins/Components/` |

Create a missing folder if necessary. Copy the whole `.vst3` or `.component` bundle, keeping its
contents intact, then restart the DAW and rescan. No setup script or Access package is needed.

Before replacing an existing version manually, move it to a backup folder outside the plug-in scan
folders. Also move **Virus TI Rework Development.vst3** out of the scan folder: it shares the VST3
identity and must not remain installed alongside the new bundle. Setup handles this backup automatically.

### Updates and uninstall

To update, quit your hosts and run **Install / Update** from the new download. Setup verifies its payloads,
backs up existing installations in `~/Library/Application Support/VirusTIRework/PluginBackups/`, and
attempts to roll back an update if installation fails. Keep those backups when reporting an update failure.

To uninstall, quit your hosts, open Setup, select the formats to remove and click **Uninstall**. Your
imported patches, optional appearance resources, recovery evidence and installation backups are kept.
For a manual uninstall, move just the installed `.vst3` and/or `.component` bundles out of their scan
folders. Removing the downloaded ZIP alone does not uninstall the plug-ins.

### If the plug-in is missing or silent

- **Not listed in the DAW:** check the format table, confirm that you copied the complete bundle into
  the correct current-user folder, restart the host and rescan. Remove duplicate Development bundles
  from scan folders. Check any scan failure or macOS approval message before retrying.
- **Loaded but silent:** wait for **Connected**, send notes on the selected part's MIDI channel,
  check track monitoring and the host's audio-output routing, and start with the main stereo return.
  The plug-in needs the physical TI; it is not a software synthesizer or emulator.
- **Device already owned:** disconnect the other instance, or wait for its restoration helper to
  finish. Installing both AU and VST3 does not let both connect simultaneously.

If that does not resolve the problem, [report a reproducible setup issue](https://github.com/Give-me-a-signal/virus-ti-rework/issues/new?template=bug-report.yml)
with the release, Mac/chip, macOS, host/version, AU or VST3 format, exact message and the stage that failed.

### Requirements and verified scope

Apple Silicon Mac, macOS 12 deployment target, an AUv2 or arm64 VST3 host, and physical Virus TI hardware.
A deployment target does not establish compatibility with every OS or host. TI Desktop is the initial hardware
validation target; TI2/Snow remain unverified. The packaged beta has bounded smoke-test results for Logic Pro,
Bitwig, Ableton Live and LiveProfessor on M1 Pro/macOS 26.2. Coverage differs by host; GarageBand remains
untested. Read the [host test report](docs/daw-beta-validation-2026-10-15.md) and [compatibility matrix](docs/COMPATIBILITY.md).

### Tested setups

The following checks used the **packaged 0.2.0-beta.1 candidate**, an **Apple M1 Pro**, **macOS 26.2
(25C56)** and a **Virus TI Desktop** on 15 October 2026. These are short checks on one setup, not a
general compatibility guarantee.

| Host | Format / host rate | Passed in this beta pass | Outstanding checks |
| --- | --- | --- | --- |
| **Logic Pro 12.4** | AU / 48 kHz | Editor/theme controls, MIDI-region playback to the TI, instrument/Stereo Out meter signal, three internal stereo returns | Project reopen/recovery confirmation, auxiliary mixer routing and automation |
| **Bitwig Studio 5.2.7** | VST3 / 48 kHz | Plug-in loading, three internal stereo returns, normal-exit RAM restoration | New beta project reopen, host MIDI-clip confirmation, editor interaction and auxiliary routing/automation |
| **Ableton Live 12.4.1 Suite** | VST3 / 44.1 kHz | Editor, computer-keyboard MIDI, three internal stereo returns, manual RAM restoration | Save/reopen/export deferred because this workstation's Live is not authorized; auxiliary routing/automation |
| **LiveProfessor 2025.1.0** | AU / 48 kHz, 512 frames | Editor/theme switch, three internal stereo returns, project save/reopen and reconnection, sound/routing recall, normal-exit RAM restoration | Host MIDI-input routing, external audio patching and automation |

All four completed return tests recorded **zero capture errors, FIFO overruns and FIFO underruns**.
Internal return telemetry confirms audio at the plug-in's outputs; it does not verify each DAW's
auxiliary-track setup or physical monitoring. Earlier Development VST3 Bitwig recall results are
recorded separately below. [Read the complete beta test scope](docs/daw-beta-validation-2026-10-15.md).

### Optional Original Access appearance

**This is optional.** Dark and Polar work without Access software. If you own the original installer
and want the familiar legacy artwork:

1. [Download the original macOS installer from Access](https://www.virus.info/download/585).
   This official link may redirect to an Access sign-in page. Sign in with your Access account, or
   [register with Access](https://www.virus.info/signup) if needed. In the software downloads, look for
   **Virus TI Installer 5.1.7.00 for macOS**, listed for **macOS 10.7–10.14**. That OS label applies
   to the legacy software, not to Virus TI Rework.
2. If the download is an archive, extract it in Finder and locate
   **Virus TI Software Suite 5.1.7.00.pkg**. **Do not open or run that `.pkg` installer.**
3. Open **Virus TI Rework Setup.app** and click **Import Original Access Appearance…**.
   Alternatively, open the appearance menu at the bottom of the plug-in editor and choose **Import Original…**.
4. In the file chooser, select your `.pkg` and wait for the import to finish. The native importer
   checks the supported package and extracts appearance resources without running the Access installer
   or vendor code. No Python, compiler or Terminal command is needed.
5. In the plug-in editor's appearance menu, choose **Original Access**. Choose **Modern UI** to return
   to the included Dark/Polar editor; its theme button switches between those two themes.

AU and VST3 share the imported artwork and appearance preference. Imported files live outside the
plug-in bundles in `~/Library/Application Support/VirusTIRework/Appearance/v1/`, so you do not need to
repeat the import for each format or after a normal plug-in update. This imports the appearance;
it does not add unfinished controls or install the legacy drivers, firmware or factory sound library.

If import is cancelled, the package is unsupported, or the imported files later go missing, Dark/Polar
remain available. Only the currently supported package with SHA-256
`3ebcfef395a6c5f1ef2162404228f1fbe571371f7389836f13c55c8a7636ce1d` is accepted.
Original Access resources retain their own terms, are imported locally by the owner, and are not
included in this project's releases.

### First hardware session

1. Preserve important sounds using your normal backup method. Connect and power on your TI Desktop
   over USB, start it in **Single mode**, and create a disposable DAW project.
2. Keep your normal DAW audio-output interface selected. Load **one** Virus TI Rework instrument instance.
   Scanning or loading a fresh instance does not connect to the synth automatically.
3. Click **Connect TI** in the editor. Wait for snapshots and initialization to finish and the status
   to report **Connected** before playing. Preserving and verifying sixteen edit buffers can take a
   minute or more; do not interrupt it or try connecting from another instance.
4. Send MIDI on **channel 1** for **Part 1**. Parts 2–16 use MIDI channels 2–16. The synth produces
   the sound; the plug-in returns it to your DAW over USB.
5. For multiple audio returns, enable the additional stereo outputs in your host and route them to
   auxiliary/audio tracks: **USB Return 1**, **USB Return 2** and **USB Return 3**. Host-specific
   routing remains part of the testing work. Browse captured hardware edit buffers or import your own
   `.mid`/`.syx` files through the patch browser; no factory bank download is required.
6. **Save the DAW project while connected** to capture the current sounds and routing. Reopening a
   connected project requests recall when realtime processing starts; verification differs by host
   as described in [tested setups](#tested-setups).
7. When finished, stop playback and click **Disconnect** to restore the original RAM, or let the
   restoration helper finish after the host exits. Keep the synth powered and connected until
   restoration completes before unplugging it or opening another connected instance.

Only one instance across AU/VST3 and the restoration helper may own the device. If the host or shared
audio device changes sample rate, stop playback, wait for restoration if indicated, and reconnect;
seamless continuation through that change has not been established. Firmware updates, stored-bank
writes and offline hardware rendering are unsupported. Follow the [staged hardware guide](docs/HARDWARE_TESTING.md).

### Building from source

Developers can run `bash tools/build_beta.sh` after installing Apple Command Line Tools, CMake 3.25+, Ninja,
Git and Python 3. This builds AU/VST3 and Setup without an Access package, runs tests and the VST3 validator,
and creates an audited ZIP in `dist/`. The build fetches pinned Steinberg and Apple SDK revisions.

### AI-assisted setup for non-technical users

You can ask a local AI agent to help with installation or collect a test report. The prebuilt download
and setup app are still the recommended route. Copy this prompt:

```text
Help me install and test Virus TI Rework from
https://github.com/Give-me-a-signal/virus-ti-rework .
Read the README and docs/HARDWARE_TESTING.md. Prefer the prebuilt GitHub beta
and graphical setup app; no Access installer is required for Dark or Polar.
Check my Apple chip, macOS, DAW and exact Virus model/firmware. Do not disable
macOS security, update firmware or write stored banks. Use a disposable DAW
project and follow the untested-model restrictions. Record pass/fail/not-tested
for installation, scan, playback, routing, recall and restoration. Let me
review a compatibility report before posting it. If building is necessary,
use tools/build_beta.sh; explain missing developer prerequisites first.
```

## What works

| Area | Implemented and tested scope |
| --- | --- |
| Native plug-in | arm64 VST3 processor/controller, AUv2 wrapper and Cocoa editor; three stereo output buses and sixteen-channel MIDI input |
| Real hardware audio | Capture through Apple's existing USB audio/CoreAudio support; six device channels mapped to USB1, USB2 and USB3 |
| Host sample rates | TI capture stays at 48 kHz; a 64-tap windowed-sinc resampler and FIFO drift correction adapt to the DAW |
| MIDI and parts | Vendor-specific TI USB MIDI interface, note delivery, supported CC controls, sixteen-part selection and routing |
| Patch browser | Hardware edit buffers and user-imported MIDI/SysEx files; search, categories, two panes and selected-part recall |
| Project recall | Versioned state contains sixteen RAM patches, RAM multi, routing and 2,672 mapped parameter values; connected projects reconnect when realtime processing starts |
| Recovery | Original RAM snapshots before changes; verified manual Disconnect; independent recovery helper after plug-in/DAW exit or host crash |
| Editor | Self-contained Modern Dark and Polar vector appearances plus optional imported original artwork, 100–200% scaling, dynamic graphs and host parameter gestures |
| Metering | Separate note-velocity activity and CC7 volume per part; three actual mixed USB capture peak meters |
| Local API | Per-user Unix socket and Python control tool; no network listener; hardware connection is explicit except restored connected projects |

The vendor-specific MIDI path is reconstructed in user space. **We do not load the old Intel vendor plug-in, install the old kernel extension, or flash the synth.** Stored-bank writes and firmware commands are outside the hardware command allowlist.

## How it works

```mermaid
flowchart LR
    DAW[Native VST3 host] -->|Notes and supported CCs| Plugin[VST3 processor]
    Plugin -->|Bounded queue| Worker[Native device service]
    Worker -->|Vendor-specific USB MIDI| TI[Virus TI hardware]
    TI -->|Six-channel USB audio| Apple[Apple USB audio / CoreAudio]
    Apple --> FIFO[Capture FIFO and resampler]
    FIFO -->|USB1 / USB2 / USB3| Plugin
    Plugin -->|Three stereo buses| DAW
    Worker --> Snap[Original RAM snapshots]
    Snap --> Helper[Independent restoration helper]
    Helper -->|Restore after host exit| TI
```

### Audio transport and driver work

Early work examined the original installer, kernel driver, shared libraries and MIDI driver to recover descriptors, packet layouts, initialization and clock behavior. We implemented bounded PCM conversion, asynchronous request lifecycles, descriptor parsing, rollback/drain behavior and clock-domain experiments as separate native modules.

The connected-device work established a simpler production route: reuse **Apple's existing USB audio transport**, then capture through CoreAudio from a native user-space service. Those earlier low-level transport modules remain useful research and test infrastructure; they are not a claim that a new kernel driver ships with this plug-in.

The production path uses a capture FIFO, 64-tap resampling and occupancy-based drift correction to bridge the TI's fixed 48 kHz stream to the host rate. The callback uses bounded queues and lock-free state; hardware operations, file access and snapshots happen on the worker. Missing audio is silenced. The reported buffering is approximately 42.7 ms, not a measured end-to-end latency figure.

Three-return operation required identifying the original **Global USB controller 9** configuration command. Hardware tests showed the third return was otherwise an input-like signal. Selecting the recovered three-return configuration fixed that path; teardown explicitly normalizes back to the standard two-return behavior.

### MIDI, state and recovery

Public MIDI discovery alone did not reproduce the original TI control path. Static analysis identified a separate vendor-specific USB interface and protocol behavior. The new service opens that interface and sends a restricted set of note, supported CC, RAM-patch, request, routing and mode messages. MIDI reaches the hardware on the worker thread; sample offsets in the VST3 bridge do not imply sample-accurate USB scheduling.

Project state stores real hardware patches, rather than merely the positions of visible knobs. Every dump is bounded and checksum-checked. Bitwig testing exposed two practical defects: overly slow duplicate reads during Save, and host state caching after API-driven edits. Serial single reads and explicit controller dirty notifications resolved the tested save/reopen path.

DAW shutdown exposed another issue: a plug-in cannot rely on being allowed to spend approximately 23 seconds restoring hardware inside termination callbacks. A separate signed helper now retains the original snapshots and exclusive device lease before changes begin. A dedicated local pipe signals normal completion or host disappearance. The helper validates device identity, restores RAM, verifies readbacks and records the outcome even when the host exits abruptly.

### Editor reconstruction

The initial reconstruction used the original XML for the 907×513 layout, control bindings, conditions and value lists. The original PNGs supplied the optional familiar appearance. REA/Ghidra analysis informed waveform display tables, graph geometry and envelope/filter behavior. A new native Cocoa view implements the interaction and VST3 bindings; it does not execute the old editor.

The resource-independent beta uses project-authored control layouts and illustrative graphs for Dark/Polar. Optional original appearance data is parsed locally at runtime. Earlier work drew modern vector controls using the recovered structural layout. Work included dynamic oscillator/filter/envelope graphics, popup semantics, patch-bank parsing, keyboard interaction and scaling. A UI sweep corrected PNG-DPI scaling that displaced oscillator controls, and menu resizing/text layout that stretched arrows or clipped values.

This is a documented reverse-engineering reconstruction, not recovered vendor source and not a clean-room claim. Graphs visualize editor parameters; they are not a software model of the TI's audio engine.

## How we built it

| Stage | Work and outcome |
| --- | --- |
| Inventory | Expanded the owner-supplied 5.1.7.00 package, hashed artifacts, catalogued architecture/framework dependencies and retained originals unchanged. |
| Static analysis | Used the REA skill/MCP and Ghidra to trace the legacy kernel, MIDI, common engine and editor paths; kept evidence IDs and decompilation snapshots locally. |
| Offline foundations | Built and tested descriptor, PCM, queue, MIDI, session and clock modules before relying on connected hardware. |
| Native VST3 | Added arm64 processor/controller, sixteen-part parameters, state validation, three stereo buses and a native editor; exercised the official validator. |
| UI and browser | Reconstructed the original layout, added graph interaction, verified 7,164 patches, corrected layout defects and added Dark/Polar appearances. |
| Real TI audio | Verified vendor-specific MIDI and USB audio, then integrated CoreAudio capture and resampling directly into the plug-in's service. |
| Multi-part validation | Auditioned all sixteen parts, verified sound/routing recall, repaired USB3 initialization and checked 44.1/48/96 kHz host rates. |
| DAW validation | Demonstrated playback and Bitwig project recall in earlier development builds; added packaged AU/VST3 checks in Logic, Bitwig, Live and LiveProfessor, including LiveProfessor AU save/reopen and restoration. |
| Exit recovery | Reproduced the quit-time RAM restoration failure, implemented the independent helper, and tested graceful exit, abrupt termination and normal Bitwig quit. |
| Reproducible builds | Separated owner-supplied resources, added reproducible local setup/packaging, MIT licensing, fresh UI gallery and source-only checks. |

Firmware/wavetable decoding and emulator experiments were a parallel research track. They helped offline investigation but are **not required to play the physical TI**, are not a firmware update feature, and the emulator/runtime sources and binaries are not bundled in this hardware repository.

## Validation and scope

The [packaged beta host results](#tested-setups) above apply to the new AU/VST3 download candidate.
The table below also retains earlier engineering runs, whose dates and revisions are in the linked
reports. A result from a Development build is not a completed retest of every stage in the new beta.

| Check | Recorded result |
| --- | --- |
| Local beta build | Nine project CTest suites passed; VST3 validator 47/47; strict auval passed; arm64 bundles/helper signatures verified |
| 48 kHz / 256 hardware matrix | Sixteen-part routing/recall, 48 isolated auditions, simultaneous 48-note chord and two reconnects passed |
| Host rate adaptation | Physical three-return checks at 44.1 kHz / 64 and 96 kHz / 512 passed; device remained at 48 kHz |
| Bitwig Studio 5.2.7 | Native save/reopen at 44.1 kHz / 256, 32 isolated auditions, sixteen sounds/multi and all 2,672 saved parameter tuples matched |
| Hardware capture counters | Recorded matrix/Bitwig windows had zero capture errors and FIFO overruns/underruns |
| Exit recovery | Graceful API-host exit, SIGKILL of the test host and normal Bitwig quit restored and verified original RAM; manual Disconnect also passed |
| User testing | Further Bitwig use reported by the owner on 14 October 2026; not presented as a new automated hardware matrix |

See [hardware matrix](docs/hardware-matrix-2026-10-12.md), [Bitwig validation](docs/bitwig-validation-2026-10-12.md), [quit restoration](docs/quit-restoration-2026-10-12.md) and [repository verification](docs/repository-verification.json). Historical notes reference raw evidence under `analysis/`; those captures, personal projects, dumps and proprietary analysis artifacts are deliberately not tracked. Reports distinguish earlier failed runs from later fixes. Earlier sanitizer results apply to their tested revisions, not every later hardware service change.

## Help test on more Macs and synths

You do not need to write code to contribute. A careful report of what works—or fails—on a different setup is one of the most useful contributions you can make.

We need coverage across **Apple silicon chip generations and variants**, **macOS releases and point updates**, **Virus TI models/firmware**, and **native AU/VST3 hosts**. Report your exact chip (including Pro/Max/Ultra where applicable), macOS version/build, DAW version, plug-in format, USB connection path and release version/commit. A result on one combination does not establish support for another. TI2 and Snow need separate compatibility investigation; the current sixteen-part Desktop path must not be assumed suitable for them.

Start with the [hardware testing guide](docs/HARDWARE_TESTING.md). It separates build/scan checks, device discovery, playback, USB returns, project recall and restoration, so a partial test can still be useful. Use a disposable DAW project and report each stage as **pass**, **fail** or **not tested**. Testing does not require updating firmware, writing stored banks, weakening macOS security or deliberately crashing your DAW.

- [Submit a hardware / macOS compatibility report](https://github.com/Give-me-a-signal/virus-ti-rework/issues/new?template=hardware-test.yml), including successful tests.
- [Report a reproducible bug](https://github.com/Give-me-a-signal/virus-ti-rework/issues/new?template=bug-report.yml).
- Check the [compatibility matrix](docs/COMPATIBILITY.md) before interpreting a report as supported hardware.
- Improve documentation, accessibility, translations or code through a [pull request](CONTRIBUTING.md).

All proposed repository changes go through a branch and PR. The review policy requires a passing `offline-core` check, an up-to-date branch, resolved review conversations and independent maintainer approval. GitHub enforcement and the maintainer activation procedure are recorded in [repository governance](docs/MAINTAINING.md); a policy file alone cannot prevent direct pushes.

## Known limitations

- Beta quality and limited hardware/host coverage. No claim that every Virus variant or modern Mac works.
- AUv2 and VST3 beta. No AAX/VST2 release and no automatic migration of old Virus Control project chunks or plug-in identities.
- MIDI scheduling is immediate on the control worker, not sample accurate. Transport/clock synchronization and measured latency compensation need more work.
- Pitch bend, pressure, program changes, complete parameter synchronization and some encodings remain unfinished. 2,672 catalog parameters do not mean 2,672 verified hardware controls.
- Local-only algorithm selectors are not hardware-sent or persisted as full synth state. Some UI surfaces are reconstructed placeholders, including the user arp pattern editor and remote templates.
- Patch-name editing, bank saving, locks, complete accessibility and exact legacy mouse/rasterization parity remain incomplete.
- Physical unplug recovery and exhaustive click/glitch testing remain incomplete. Bitwig's warning about a parameter advertised both for automation and MIDI remains unresolved.
- No offline hardware rendering, firmware flashing or stored-bank writes. The optional emulator research is separate from this release.

## Interface gallery

Fresh **14 October 2026** captures from the real native Cocoa editor render path: twelve pages in each appearance, 36 images total. These are reproducible offscreen view captures, not design mockups or screenshots of a connected DAW session. Test patch names and meter values are deliberate fixtures; they do not imply live hardware activity. Native footer controls are not included in these offscreen captures. Remote and Preferences show the currently reconstructed surfaces, not a claim that all original features work.

<details>
<summary>Modern Dark — all twelve views</summary>

### Easy

![Modern Dark: Easy](docs/screenshots/dark-page-00.png)

### Patch browser

![Modern Dark: Patch browser](docs/screenshots/dark-page-01.png)

### Oscillators

![Modern Dark: Oscillators](docs/screenshots/dark-page-02.png)

### Filters and envelopes

![Modern Dark: Filters and envelopes](docs/screenshots/dark-page-03.png)

### LFOs

![Modern Dark: LFOs](docs/screenshots/dark-page-04.png)

### Modulation matrix

![Modern Dark: Modulation matrix](docs/screenshots/dark-page-05.png)

### Arpeggiator

![Modern Dark: Arpeggiator](docs/screenshots/dark-page-06.png)

### Effects 1

![Modern Dark: Effects 1](docs/screenshots/dark-page-07.png)

### Effects 2

![Modern Dark: Effects 2](docs/screenshots/dark-page-11.png)

### Common

![Modern Dark: Common](docs/screenshots/dark-page-09.png)

### Remote (incomplete)

![Modern Dark: Remote (incomplete)](docs/screenshots/dark-page-08.png)

### Preferences / utility (incomplete)

![Modern Dark: Preferences / utility (incomplete)](docs/screenshots/dark-page-12.png)

</details>

<details>
<summary>Modern Polar — all twelve views</summary>

### Easy

![Modern Polar: Easy](docs/screenshots/polar-page-00.png)

### Patch browser

![Modern Polar: Patch browser](docs/screenshots/polar-page-01.png)

### Oscillators

![Modern Polar: Oscillators](docs/screenshots/polar-page-02.png)

### Filters and envelopes

![Modern Polar: Filters and envelopes](docs/screenshots/polar-page-03.png)

### LFOs

![Modern Polar: LFOs](docs/screenshots/polar-page-04.png)

### Modulation matrix

![Modern Polar: Modulation matrix](docs/screenshots/polar-page-05.png)

### Arpeggiator

![Modern Polar: Arpeggiator](docs/screenshots/polar-page-06.png)

### Effects 1

![Modern Polar: Effects 1](docs/screenshots/polar-page-07.png)

### Effects 2

![Modern Polar: Effects 2](docs/screenshots/polar-page-11.png)

### Common

![Modern Polar: Common](docs/screenshots/polar-page-09.png)

### Remote (incomplete)

![Modern Polar: Remote (incomplete)](docs/screenshots/polar-page-08.png)

### Preferences / utility (incomplete)

![Modern Polar: Preferences / utility (incomplete)](docs/screenshots/polar-page-12.png)

</details>

<details>
<summary>Original Access artwork — all twelve views</summary>

### Easy

![Original Access artwork: Easy](docs/screenshots/original-page-00.png)

### Patch browser

![Original Access artwork: Patch browser](docs/screenshots/original-page-01.png)

### Oscillators

![Original Access artwork: Oscillators](docs/screenshots/original-page-02.png)

### Filters and envelopes

![Original Access artwork: Filters and envelopes](docs/screenshots/original-page-03.png)

### LFOs

![Original Access artwork: LFOs](docs/screenshots/original-page-04.png)

### Modulation matrix

![Original Access artwork: Modulation matrix](docs/screenshots/original-page-05.png)

### Arpeggiator

![Original Access artwork: Arpeggiator](docs/screenshots/original-page-06.png)

### Effects 1

![Original Access artwork: Effects 1](docs/screenshots/original-page-07.png)

### Effects 2

![Original Access artwork: Effects 2](docs/screenshots/original-page-11.png)

### Common

![Original Access artwork: Common](docs/screenshots/original-page-09.png)

### Remote (incomplete)

![Original Access artwork: Remote (incomplete)](docs/screenshots/original-page-08.png)

### Preferences / utility (incomplete)

![Original Access artwork: Preferences / utility (incomplete)](docs/screenshots/original-page-12.png)

</details>

Regenerate the gallery after building with:

```sh
/usr/bin/python3 tools/capture_gallery.py native/vst3/build-beta
# Add --original after importing your own Original Access appearance.
```

## Packaging and binary releases

`tools/build_beta.sh` builds the resource-independent AU/VST3 and graphical setup app, runs the offline
suites and VST3 validator, and produces a checksum-indexed ZIP. The macOS CI additionally runs `auval`.
Version tags on reviewed `main` publish beta prereleases only after CI passes. See
[binary architecture](docs/BINARY_ARCHITECTURE.md) and [beta release notes](docs/BETA_RELEASE.md).
Developer ID signing and notarization remain future distribution work.

Developer follow-up: [ownership/recall findings](docs/collaborator-findings-5.md) and [experimental marker-timing design and test limits](docs/marker-clock-integration.md). Marker timing is disabled by default pending hardware validation; these drafts do not change the released beta.
