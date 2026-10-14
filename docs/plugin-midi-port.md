# Native plug-in and MIDI reconstruction

This is the second-pass architecture record. [Third-pass progress](pass3-progress.md) now confirms the shared-memory IPC and public/private packetization, adds native MIDI codecs, and links the built arm64 VST3 development plug-in. Earlier unresolved statements below are retained as historical scope, not the latest status.

11 October 2026. Scope: offline examination of the extracted 5.1.7.00 package, specifically x86_64 symbol tables, linked libraries, string sections, plists, parsed XML resources, and focused REA/Ghidra traces supplied by the coordinating investigation. No vendor code was executed. No USB device was present. The local inventory worker did not change the coordinating worker's active REA target.

The practical target is a shared native control/audio engine exposed through AU and VST3, plus native CoreMIDI endpoints. This is a source reconstruction effort, not a bundle-header edit. The shipped editor contains much more recoverable structured data than the initial binary inventory showed: parameter definitions, MIDI message templates, automation indices and UI layouts are present as XML. They reduce guesswork but do not supply the original implementation or prove the hardware transport.

## Evidence and reproduction

Run `/usr/bin/python3 tools/inventory_plugin_midi.py` from the project root. The tool only reads payloads and invokes `nm`/`otool`; it never loads a plug-in. It produced:

* **PM-1:** [plugin-midi-symbol-inventory.json](../analysis/plugin-midi-symbol-inventory.json): nine binaries, exact paths and SHA-256 identities, x86_64 symbols, dependencies, VM-addressed `__cstring` sections and bundle metadata. All 27 inspection commands succeeded.
* **PM-2:** [plugin-parameter-catalog.json](../analysis/plugin-parameter-catalog.json): seven XML files, source hashes, parsed parameter sets, value lists, conditions, initialization declarations, MIDI template trees, automation rows and UI parameter bindings. Attribute values remain strings; the extraction does not silently invent semantics for the vendor XML language.

PM labels are local artifact references, not REA Evidence IDs. Existing architecture evidence is in [installer findings](../analysis/installer-findings.md). Each of these plug-ins contains i386 and x86_64, with no arm64 slice.

## What shipped

| Component | Direct observations in x86_64 | Implication, still requiring call-flow confirmation |
| --- | --- | --- |
| VST `.vst` | `VSTPluginMain` and `main_macho` exports; REA confirms creation of common CFBundle and lookup/call of `VSTPluginMainEx`; imports from `libVirusUSB` | Wrapper boundary established; common control implementation still needs reconstructing |
| VST3 `.vst3` | `GetPluginFactory`, `bundleEntry`, `bundleExit` exports; common bundle and `CreateCVSTPlugInEx` strings; Steinberg parameter-class RTTI | Existing VST3 adapter appears to wrap shared control implementation |
| AU `.component` | `AUWrapperEntry` and `AUWrapperVSTCallback` exports; common bundle and `CreateCVSTPlugInEx` strings; Cocoa, AudioUnit and AudioToolbox dependencies | Existing AU is a wrapper around the shared implementation; modern AU registration and host integration need rebuilding |
| Common `Virus Control.bundle` | `VSTPluginMainEx` and `CreateCVSTPlugInEx` exports; `VirusClient` imports for start/stop, sample rate, latency, device discovery and control; `getChunk`/`setChunk` diagnostic strings | Main editor/control/state work lives here; rebuilding only wrapper entry points will not provide native functionality |
| MIDI Driver 1.8.2.0 | CFPlugIn factory `NewTIUSBMIDIDriver`; CoreMIDI device/entity/packet imports; IOKit USB plug-in interface imports; public/hidden-port and shared-memory diagnostic strings | Separate public MIDI and private control transport are strong candidates for reconstruction; neither path is fully specified yet |

VST/VST3 and common control have the absolute installed `libVirusUSB.dylib` dependency. AU has no direct such load command in its inspected slice, but the common implementation it references does. This matters when deciding where to replace the transport.

REA/Ghidra decompiled VST `VSTPluginMain` at `0x9840` and confirmed the exact common bundle path is passed to `CFBundleCreate`, `VSTPluginMainEx` is looked up, and the function is called with `(hostCallback, 0)`; it returns null if unavailable. Evidence: `ev_167a391b15509229e9f48a44e9a5840cdf328c9ed1d06278e9a067876d333886`. Analyzed x86_64 slice SHA-256: `3500add90ef39fe1d60132146919cce9dd6cb6d2adba050403f1b124e55c9914`.

REA also decompiled AU constructor-like function `FUN_0000a02e`: it creates the same CFBundle, resolves `CreateCVSTPlugInEx`, and calls it with `(_AUWrapperVSTCallback, 0)`. This confirms the AU's common-engine factory boundary. Evidence: `ev_d21945b268fbf3aca0c3c2cc7bf32b9ca4e9a9e315773a4e3e5edc7d31ab24fc`; AU x86_64 slice SHA-256 `4bdea3b9a2ee49094387d1f5d0b742dc22b6116a9102be3cab9be6e3db1c54ca`. See `analysis/rea-au-pass2-evidence.json`. VST3 common-engine linkage remains an inference from imports and strings.

The common bundle imports Cocoa `NSView`, `NSWindow`, `NSEvent` and CoreGraphics drawing functions alongside Carbon event APIs. Thus “the GUI is entirely Carbon” is not supported. Its UI still needs native implementation work, including rendering, host-view embedding, focus, resizing and threading; a dependency list does not establish which particular API prevents loading.

## Recoverable editor and MIDI data

All following observations are parsed XML declarations in PM-2, not binary execution:

| Resource | Recoverable data |
| --- | --- |
| `remotedevice.xml` | Device attribute `Virus TI Desktop/Polar/Keyboard`; 79 parameter sets containing 587 parameter entries, 128 value lists, 123 conditions and 12 MIDI message templates |
| `remotedevicePR.xml` | Snow variant with the same counts but different contents and hash |
| `automation.xml` | 2,672 automation declarations: 167 each for parts 1–16; explicit indices, no duplicate indices in this file |
| `automationPR.xml` | 668 automation declarations across parts 1–4; no duplicate indices |
| `editordevice.xml` | 141 editor parameter entries, 35 value lists, 212 conditions and two MIDI templates |
| `editor.xml`, `editorPR.xml` | UI prototype/layout trees, image references and named parameter bindings |

For example, the Desktop parameter definition declares `Filter 1/Cutoff` with range 0–127, default 127, default message `cc`, controller index 40 and `automatable=true`. Its visibility depends on a named condition. Other parameters share indices according to mode. The MIDI trees explicitly include CC, poly-pressure, bank parameter messages and single/multi dump layouts. These are useful reconstruction specifications; USB packet framing, checksum handling, initialization sequence, mode changes and actual MIDI dispatch require further tracing and hardware validation.

The common binary contains both “loading parameter file” and “using compiled parameter definition” strings. Consequently, the XML's exact runtime precedence and equivalence to the compiled definitions remain unknown. Its embedded resource version/date fields also predate package 5.1.7.00. We must compare selected parameters and serialization with the code before treating the catalog as a complete current protocol specification. Automation file indices are not yet proven identical to VST3 parameter IDs or AU addresses. Preserving this data alone does not make old DAW sessions load.

## MIDI is a separate transport investigation

The inspected MIDI driver's dependency list does **not** include `libVirusUSB.dylib`, and its symbol table contains no `IOConnect*` or `IOServiceOpen` imports. It instead imports `IOCreatePlugInInterfaceForService` and describes `IOUSBDevice`/`IOUSBInterface`, `ReadPipeAsync`, pipe stall handling, public reads and hidden-port reads in its string section. Absence is bounded to this slice's load commands and named imports; it does not establish complete independence from audio configuration or other services.

The driver imports `MIDIDeviceCreate`, `MIDIDeviceAddEntity`, `MIDIPacketListAdd` and `MIDIReceived`. Its plist maps factory UUID `B91C94BC-7E2B-4896-897F-1A17A3B22403` to `NewTIUSBMIDIDriver` under type UUID `ECDE9574-0FE4-11D4-BB1A-0050E4CEA526`.

Strings identify `CreateHiddenPortInterface`, `FindHiddenPortPipes`, `HandleHiddenPortInput`, `HiddenPortSend`, and `VirusTICommMem`/`VirusTIMDP%08x`/`VirusTIMDW%08x`/`VirusTIMDR%08x`. Imports include POSIX shared memory, mapping, semaphores and local CFMessagePort creation. The common bundle has matching `VirusTICommMem`, remote CFMessagePort imports and diagnostics about a MIDI-driver write ring buffer. Together these support an **inference** of shared-memory IPC between the control engine and MIDI driver, carrying a private USB control path. Sizes, synchronization rules, packet contents and runtime ownership are unresolved.

Focused REA traces now establish part of the private interface selection. `CreateHiddenPortInterface` at `0x5cb0` requests class `0xff`, subclass `0`, protocol `0`, alternate setting `0`, skips the first iterator result and constructs an interface from the second. `FindHiddenPortPipes` at `0x6e70` iterates pipe indices from 1 through the endpoint count and assigns IN/OUT pipes by direction. This does **not** justify hardcoding an interface number or endpoint address: the live descriptor and iterator results must be validated first. Evidence: `ev_9cb80c438744a536128b9631d33ee5d5ad6d3253a47aa95707f73acfb3cd7dd7` and `ev_3ddedf379fa6b91c8269e9b87ef3c88eeaf12d9f28f3c4d6fd1dfbca74a0c54d`, retained in [MIDI REA evidence](../analysis/rea-midi-pass2-evidence.json). Analyzed MIDI x86_64 slice SHA-256: `52a173fd23b2345b4d32f9752144c2b66fd33f55a787564c4063ae5c7851534f`.

A native public MIDI service could therefore be an earlier milestone than complete audio, while the private editor path is reconstructed in parallel. That is a sequencing proposal, not a conclusion that built-in class MIDI will work. Actual USB class descriptors and interface claims must be captured from the connected unit. For a development service, modern CoreMIDI virtual sources/destinations can make endpoints visible to clients; this does not itself implement USB transport. Apple's destination documentation distinguishes application virtual endpoints from driver-created device entities and recommends the newer protocol-aware creation API. [Apple CoreMIDI](https://developer.apple.com/documentation/coremidi/mididestinationcreate(_:_:_:_:_:))

## Native conversion plan

1. **Shared engine first.** Separate device ownership, USB audio scheduling, private control messages, patch state and public MIDI. Explicit queues and an IPC boundary should keep USB operations and UI/serialization off the audio callback. This is a new architecture proposal; it does not claim to reproduce the old ABI.
2. **Public MIDI and parameter catalog.** Establish public endpoint framing, timestamps, long SysEx handling, reconnect and multiple-client behavior. Use the parsed catalog to implement named parameters and mode-dependent ranges, then validate selected parameter updates on hardware.
3. **AU and VST3 adapters.** Implement buses, MIDI/events, parameter automation, latency reporting, state save/restore, transport start/stop and a native editor over the common engine. AU on arm64 must use Audio Component or Audio Unit Extension APIs; Apple explicitly excludes legacy Carbon Component Manager loading for arm64. A native AUv2 component is an option; AUv3 is not inherently required. [Apple audio-porting guidance](https://developer.apple.com/documentation/apple-silicon/porting-your-audio-code-to-apple-silicon)
4. **VST3 as the first VST target.** Current Steinberg tooling supports Apple silicon and includes AU/AUv3 wrappers. This gives a practical shared-format development route but does not port the vendor implementation for us. Host format support still needs checking against the user's DAW. [VST SDK](https://github.com/steinbergmedia/vst3sdk), [Steinberg wrappers](https://steinbergmedia.github.io/vst3_dev_portal/pages/What%2Bis%2Bthe%2BVST%2B3%2BSDK/Wrappers/Index.html)
5. **Old-session compatibility separately.** Recover legacy plug-in identities, chunk versions/encoding, automation mapping and patch selection before promising old projects will reopen. A new plug-in that controls the same hardware is easier than a drop-in binary/state replacement. The current inventory has no recovered state schema.

Relative work: native AU/VST3 entry points are bounded adapter work; a complete editor, automation and recall system is substantial but aided by XML; long-running USB audio with correct timing remains the critical hardware validation task. No reliable calendar estimate is possible from symbol inventory alone.

## Exact next REA targets

Paths below are relative to `analysis/package-expanded`. Thin x86_64 copies may be needed by the provider. Addresses are x86_64 VM addresses from `nm`/`otool`, not universal-file offsets.

| Target binary | Function / string seed | Question |
| --- | --- | --- |
| `Core_components.pkg/Payload/Library/Audio/Plug-Ins/VST/Virus TI.vst/Contents/MacOS/Virus TI` | `VSTPluginMain` `0x9840`; `VSTPluginMainEx` string `0x21cf7` | Wrapper call confirmed; follow constructor into common implementation |
| `Core_components.pkg/Payload/Library/Audio/Plug-Ins/Components/VirusTI.component/Contents/MacOS/VirusTI` | `AUWrapperEntry` `0xc1f0`; `AUWrapperVSTCallback` `0xaf50`; constructor-name string `0x36f07` | AU dispatch, properties, event/state conversion and common object interface |
| `Core_components.pkg/Payload/Library/Audio/Plug-Ins/VST3/Virus TI.vst3/Contents/MacOS/Virus TI` | `GetPluginFactory` `0xaa00`; constructor-name string `0x37c67` | Class IDs, processor/controller structure and legacy parameter-index translation |
| `Core_components.pkg/Payload/Library/Application Support/Access Music/Virus TI/Common/Virus Control.bundle/Contents/MacOS/Virus Control` | `VSTPluginMainEx` `0x57bc0`; `CreateCVSTPlugInEx` `0x57c50` | Shared class creation and host callback ABI |
| Same common binary | `setChunk` diagnostic string `0x10c280`; `getChunk` `0x10c2f0` | Find callers and reconstruct state encoding/versioning |
| Same common binary | parameter-file string `0x110eb3`; compiled-definition string `0x110ed0`; `VirusTICommMem` `0x116b2e` | Determine XML/compiled precedence and IPC ABI |
| `Driver_package_10.9.pkg/Payload/Library/Audio/MIDI Drivers/Virus TI MIDI Driver.plugin/Contents/MacOS/Virus TI MIDI Driver` | `NewTIUSBMIDIDriver` `0x36e0`; `CreateHiddenPortInterface` string `0x1424b`; `FindHiddenPortPipes` `0x14478`; `HandleHiddenPortInput` `0x145ae`; `HiddenPortSend` `0x14816`; `VirusTICommMem` `0x149a5` | Trace factory/vtables, interface matching, public and private framing, buffers, timestamps and IPC |

The latter names are **string seeds**, not preserved function symbols; their containing functions need recovery. This pass has partially answered architecture and conversion scope, recovered useful declarative data, and identified the next trace points. Plug-in execution, MIDI protocol correctness, state compatibility and native end-to-end operation remain unverified.
