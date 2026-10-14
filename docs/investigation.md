# Virus TI 5.1.7.00: initial investigation

Investigation date: 11 October 2026. Requested outcome: full USB audio, MIDI and DAW integration for the user's Virus TI Desktop on Apple silicon. The user will connect the hardware later. DAW remains unspecified. The inspected development host reports macOS 26.2, build 25C56, arm64.

This document records the initial pass. See [third-pass progress](pass3-progress.md) for the native asynchronous audio engine, recovered public/private MIDI framing and built VST3 development plug-in. [Second-pass progress](pass2-progress.md) covers descriptor acquisition and PCM/queue/pacing code; [plug-in/MIDI findings](plugin-midi-port.md) covers XML resources and wrapper boundaries. End-to-end milestones below remain open.

## Finding

This is a driver and application reconstruction project. Editing the installer alone cannot supply native CPU code or replace its dependencies on the old USB kernel stack. There is sufficient surviving symbol information to pursue a reconstruction, but successful USB streaming has not been demonstrated.

The initial pass inventoried every regular extracted file, traced selected control and audio functions with REA/Ghidra, and built a native discovery utility. It did not recover the whole application source, port the plug-in UI, establish the full USB protocol, or validate timing against hardware.

## Artifact identity and extraction

Original package: `Virus TI Software Suite 5.1.7.00.pkg`, 34,870,209 bytes.

SHA-256: `3ebcfef395a6c5f1ef2162404228f1fbe571371f7389836f13c55c8a7636ce1d`.

REA package evidence: `ev_3ab4f79fe9f66f393ad18e49dd500240e85228451634872abf3a7f1f0f487e96`.

REA's current PKG adapter only established the root identity. Children were extracted separately using Apple's `pkgutil --expand-full` into `analysis/package-expanded`, without installation or script execution. `tools/inventory_package.py` records paths, SHA-256, Mach-O headers, plist contents, installer scripts, and native `file`/`otool` output. These are local-tool observations, not REA child-inventory claims.

The inventory contains 824 regular files and 21 Mach-O files. No detected Mach-O contains an arm64 slice. Architecture and dependency details appear in [installer findings](../analysis/installer-findings.md).

| Component | Observed issue | Work implied |
| --- | --- | --- |
| Installer | Intel CPU gate; old driver destinations; updater section | Replace packaging after native components work |
| AccessVirusTI.kext (10.9 variant, version 3.3.11) | x86_64 only; IOUSBFamily and IOUSBDevice dependency | Reimplement transport against supported native USB APIs |
| libVirusUSB | x86_64/i386; kernel user-client calls; shared audio buffers | Reconstruct transport/API boundaries |
| MIDI and HAL plug-ins | Intel only; vendor driver dependency | Native MIDI and CoreAudio integration |
| AU/VST/VST3 and common control bundle | Intel only; absolute libVirusUSB dependency | Native plug-in/editor/state integration or a separately proven bridge |
| Virus Control Center | i386 only | Rewrite required for native modern macOS use |
| RTAS | i386 only | Legacy format; modern DAW format must be selected |

Many binaries import Carbon or libstdc++; their current runtime compatibility was not tested. Dependencies by themselves are not proof that a particular symbol is missing.

## Evidence-backed transport map

Two principal native targets were analyzed:

1. `libVirusUSB.dylib`, 10.9 driver variant. REA rejected its Intel-only universal container on this arm64 host. A separate x86_64 slice was created with `lipo -thin`, then successfully analyzed. Slice SHA-256: `a65cdd0d3419949d1c64ec51fe84b10261128fc6ffe0e859769549a23ec4411f`. The original universal file was not modified.
2. `AccessVirusTI.kext/Contents/MacOS/AccessVirusTI`, already thin x86_64. SHA-256: `7308f591d634625acfaa0019f218af99e04cf936b4dfe18227a0bbf28a67e539`.

Ghidra 12.1.3 supplied the pseudocode. REA notes its bridge was verified against 12.1.4 and accepts other 12.1.x builds. Recovered types, signatures, indirect calls and some function boundaries need corroboration. Pseudocode is not original source and has not been executed.

| Static observation | Function/address | REA evidence |
| --- | --- | --- |
| User-space wrapper passes control requests to PGOSXDevice | VirusClient::vc_request, library `0x2290` | `ev_eeba9c00fae606222545f1461d67fe2fbe497aaecf31d7531e9bb0318c781e51` |
| Control wrapper uses request 0x0d and a payload of at most 128 bytes for nonnegative lengths | PGOSXDevice::deviceRequest, library `0x74f0` | `ev_f454cc8853b495f616931c2f68dbfcd2f5d7e5f96984d0ff8b1ef4ef36bbd8a9` |
| IOConnectCallStructMethod selector 0 carries a 0x10c-byte in/out structure | DriverConnection::callDriver, library `0xdd80` | `ev_8ba3ca2aa1585e0baef61b9a117157be47ab858c3253efb5427a534a52595d11` |
| Dispatch branches configure, start, stop and control requests | AudioUserClientVirus::userDispatch, kernel `0x3db4` | `ev_4d11d8205a2779deda92eed72d94f0196b0ad356d7653a1044bd86c87517af2f` |
| USB request type is OR of direction, type and recipient | PGKernelDeviceVirus::deviceRequest, kernel `0x1cbaa` | `ev_dd7be0085a2ab5ff67d17c9cce506d7cffad16fbc5c6824b4e0c1ccf75853001` |
| Frequency uses class/endpoint SET_CUR, value 0x0100, three little-endian bytes | PGKernelDeviceVirus::setFrequency, kernel `0x1ed52` | `ev_93c97d7fc814a2344ebae8a1de3e8e999588a8969ad0284f9d290a868f4acad6` |
| Interface selection defaults to 44.1/48 kHz, six host input channels, two output channels, 16 bits, then applies feature overrides and fallback searches | PGDevice::findInterfacesInConfig, kernel `0xd59a` | `ev_40bc57c807dc52751afa5da75551bfefdc5b45d5f8104b7ee1e107f3234d5425` |
| Streaming chooses pipes and converters from selected descriptors, then configures device | PGKernelDeviceVirus::configureDevice, kernel `0x1d6e6` | `ev_3334ac32adef7830620c887d497dccf981bc4d430274ca0de7ca7765900d356b` |
| Input processing decodes frames, observes frame patterns and writes audio/timestamps | PGDevice::rtsProcessIsocIn_Std, kernel `0x181e4` | `ev_047055d2a9bdd24e50c2497f3da5eb63dec337305e34237735d61e80e414692b` |
| Packed 24-bit little-endian PCM maps to/from the upper 24 bits of internal 32-bit samples | pcmFrom24LSB / pcmTo24LSB, kernel `0xa5cc` / `0xa621` | `ev_aa56aaea6fb79accca77410bd6a0d8eafecc68defde450bf4063346120ecfdbe` / `ev_550f85e629d1ca56557639b887b3ae878a349780f45cb662eecccc816bf5291b` |

The shipped Info.plist declares VID `0x133e`, PID `0x0815` or `0x0816`, and class `PGKernelDeviceVirus`. These identify the initial probe matches; mapping each PID to a physical model or operating mode remains unverified.

The driver includes substantial shared multi-vendor code. `VendorAsyncPort::onFindInterfaces` and `onConfigureDone` specifically gate their described interface on Elektron IDs. `pcmTo24Access` selection includes PID `0x6000`, not the two matched TI PIDs. Do not infer TI behavior from names or copy every vendor branch. The observed six/two channel defaults and PCM routines do not establish the actual negotiated USB mode of this user's unit.

### Partial legacy dispatch layout

Offsets below are within the 0x10c-byte structure, recovered by reconciling the library wrapper and kernel switch. They are analyst-derived field interpretations, not a complete ABI specification or a new transport design.

| Offset | Size | Meaning in request 0x0d |
| --- | --- | --- |
| 0x00 | 4 | Request number |
| 0x08 | up to 128 | Control payload |
| 0x88 | 4 | Payload length |
| 0x8c | 2 | USB wValue |
| 0x8e | 2 | USB wIndex |
| 0x90 | 4 | Direction, 0 or 0x80 |
| 0x94 | 4 | Request type bits |
| 0x98 | 4 | Recipient bits |
| 0x9c | 1 | bRequest |

Observed request numbers include 1 configure, 2 start, 3 stop, 4 device-info chunks, 0x0d control, 0x0f allocated bandwidth and 0x10 free bandwidth. Other branches remain in the retained pseudocode. These are kernel user-client operations, not USB bRequest values.

## Implementation direction

Use a native USB proof of concept to validate transport before committing to system-wide driver packaging. Apple's [IOUSBHost](https://developer.apple.com/documentation/iousbhost/) supports application access to custom USB audio/MIDI devices and isochronous transfers. That is an API capability, not proof that its timing or interface ownership will work with this hardware.

For system-wide DAW audio, investigate [AudioDriverKit](https://developer.apple.com/documentation/audiodriverkit) with [USBDriverKit](https://developer.apple.com/documentation/usbdriverkit), or a native audio-server plug-in communicating with a user-space transport if supported and justified by prototype results. DriverKit requires [appropriate Apple entitlements](https://developer.apple.com/documentation/driverkit/requesting-entitlements-for-driverkit-development); approval for this project and Access's VID is unresolved. DriverKit is a candidate architecture, not an approved deliverable.

Rosetta does not translate kernel extensions and cannot mix ARM and Intel code in one process. Thus it cannot make the existing Intel kernel driver usable; an Intel plug-in bridge would still need a replacement transport and independent compatibility testing. See [Apple's Rosetta guidance](https://developer.apple.com/documentation/apple-silicon/about-the-rosetta-translation-environment).

Access's [official Catalina warning](https://www.virus.info/news) describes broken USB audio transport/CoreAudio and Control Center already at Catalina. The inspected evidence does not establish a 2021 end-of-support date. The CPU transition is only one of the compatibility changes.

## Milestones and acceptance criteria

1. **Artifact inventory — complete within scope.** Retain hashes, architecture inventory, installer behavior and selected decompilation.
2. **Native discovery — implemented; disconnected case tested.** Connected case must record actual VID/PID, device version, interface information and current endpoints.
3. **Descriptor and control transport — partial static mapping only.** Capture complete device/configuration/alternate-setting descriptors. Establish open/close ownership and bounded read requests. Validate frequency request and mode transitions against the hardware before sending writes.
4. **USB audio engine — unimplemented.** Prove endpoint selection, framing, channel order, sample representation, sample-rate negotiation, queue depth, clock feedback, timestamps and under/overrun recovery. Match 44.1/48 kHz modes to real descriptors and test long-running bidirectional audio, reconnect and sleep/wake.
5. **MIDI/control — unimplemented.** Recover endpoint/protocol framing and state management, then test patch recall, automation and multi-part operation alongside audio.
6. **DAW integration — unimplemented.** Select target DAW and plug-in format, rebuild editor/state handling, expose correct buses and latency, and verify project reopen and offline/online transitions.
7. **Packaging/distribution — unimplemented.** Resolve native driver route, entitlements where needed, signing, installation/removal and clean-host validation. No firmware update is required by the current reconstruction plan.

The hardware being disconnected prevents descriptor and end-to-end transport tests; it does not prevent more static analysis. The next targeted static functions are `PGDevice::tryFindInterfaces`, feature initialization, MIDI pipe assignment, frame-pattern observation, stream start/stop and real-time request scheduling. Resume the kernel with REA open_binary using the exact original extracted path and `analysis/AccessVirusTI.rea-snapshot.json`.

## Validation and limits

The inventory script completed with all 63 native inspection commands successful. Native probe compilation used arm64 and warnings-as-errors. Its initial run returned valid JSON with successful USB/MIDI/audio queries and no matching hardware, consistent with the user's disconnected device. `--require-device` returned 3; an invalid argument returned 2.

Transport primitive tests check byte layouts and malformed inputs offline. Their success cannot establish USB transport compatibility. See the module README for the exact test coverage.

No installer, shipped driver, firmware updater or vendor application was launched. No hardware request was sent, no system driver installed and no security setting changed. Full USB audio and DAW behavior remain unverified and unfinished.
