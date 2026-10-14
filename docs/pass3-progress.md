# Third pass: native engine, MIDI control and VST3

11 October 2026. All work was offline on the Apple-silicon Mac. The Virus was not connected. The original installer and firmware were not executed or modified. These are new native components informed by static analysis, not recompilations of recovered vendor source.

## Working development artifacts

- [Audio engine](../native/audio-engine/README.md): asynchronous request lifecycle, bounded PCM queues and request storage, ordered retirement of out-of-order completions, cancellation/drain, stale-token protection, packet/error accounting and timestamps. The actual macOS 12+ IOUSBHost adapter compiles for arm64 and submits isochronous transactions through injected, already-opened pipes. Native and ASan/UBSan tests passed, including 9,600 terminal callbacks across completion permutations, 120 stop interleavings and Objective-C adapter test doubles. No USB object was opened or used. These tests establish software behavior, not USB timing or device compatibility. The reconstructed legacy frame observer remains separate from the nominal playback scheduler until clock behavior is validated.
- [MIDI core](../native/midi-core/README.md): bounded MIDI parsing, exact short XML message builders, private ring snapshot inspection, public USB-MIDI packet codecs and a separate private timing-marker codec. Native arm64 and ASan/UBSan tests passed, including 1,024,000 parser stress bytes, 2,047 SysEx lengths and all 16,384 raw private timestamp values. Public MIDI and private vendor words have distinct validation rules.
- [Native VST3](../native/vst3/README.md): actual arm64 processor/controller bundle, 2,672 catalog parameters across 16 parts plus connection status, three proposed stereo returns, event input, automation storage and a new versioned project-state schema. The official Steinberg validator passed 47 tests with zero failures. Project Release and ASan/UBSan tests passed, including every truncated state length. See [verification record](../native/vst3/verification.json).

The VST3 is explicitly **disconnected and silent**. It has no custom editor, device service, legacy-state compatibility or sample-accurate hardware automation. Forty-eight unresolved softknob rows remain read-only. It is built locally, not installed into the system plug-in directories. AU remains a later native adapter over the same engine.

## MIDI driver: observed private IPC

Target: `analysis/thin/VirusTIMIDI.x86_64`, SHA-256 `52a173fd23b2345b4d32f9752144c2b66fd33f55a787564c4063ae5c7851534f`.

`AllocateFreeCommSlot` at `0x62b0` maps 16 KiB named `VirusTICommMem`. The recovered global header contains version 2 at offset 0 (u16), size 0x4000 at offset 4 (u32), and 16 slots at offset 8 (u16). Slots begin at offset 12 with stride 0x310. Each slot contains a device-address field at +4, a message-port name `VirusTIMDP%08x` at +8, write-buffer name `VirusTIMDW%08x` at +0x108, and read-buffer name `VirusTIMDR%08x` at +0x20c. The formatting argument is the slot index, not the hardware serial number.

Read and write buffers are each 4096 bytes. Their little-endian header contains read index +0, write index +4 and capacity +8; words begin at +12. Capacity is 1021 words, with one reserved slot and therefore 1020 usable entries. Empty means read equals write; full means next(write) equals read. Read and write both advance modulo capacity. This is now corroborated on both producer and consumer paths. The new snapshot inspector reads immutable copies; it does not attach to legacy live shared memory.

Private input `0x71a0` holds a semaphore and copies complete four-byte words into the read ring. Output filler `0x7de0` holds a semaphore and drains whole words into a USB buffer. Writer `0x7cd0` submits those bytes without interpreting their payload. Eight output buffers rotate through callback `0x7ec0`. These traces establish transport structure; they do not prove endpoint identity on the user's unit or operational behavior on modern macOS.

Relevant pseudocode: [slot allocation](../analysis/decompiled-pass3/midi-0x62b0.c), [read allocation](../analysis/decompiled-pass3/midi-0x6720.c), [write allocation](../analysis/decompiled-pass3/midi-0x6910.c), [ring initialization](../analysis/decompiled-pass3/midi-0x8600.c), [ring read](../analysis/decompiled-pass3/midi-0x81c0.c), [ring write](../analysis/decompiled-pass3/midi-0x80d0.c), [USB output filler](../analysis/decompiled-pass3/midi-0x7de0.c). Each file retains its exact evidence ID and source hash.

The CFMessagePort callback at `0x7930` accepts `stop` (integer 0x73746f70) and `st01` through `st09` (0x73743031–0x73743039). The latter select float intervals of approximately 0.0005 through 0.0045 seconds, in steps of 0.0005; stop sets zero. Recovery required the raw jump table at 0x7b78 and float constants 0x15568–0x1558b because the decompiler omitted the dispatch case bodies. Table evidence: `ev_2124d7df275372656344f103c3916fe6fb34ec4c55c564e1f5b63ec98f6be94a`; case bytes: `ev_433030257c81d8e237b4fe3becb788e9437e7aaab8ce865766c681eb4ebb1e1e`; float bytes: `ev_07f4cd517fe304134d37e0e88c1cb67b672289edf59eaff4d86c889374e9af45`. Timer callback `0x7ba0` uses this interval for continued sends. This is a send cadence, not a proven USB audio clock period.

## Public USB MIDI framing

The public receiver at `0x109a0` examines four-byte entries. The upper header nibble chooses a cable/port; the lower nibble chooses payload length and SysEx handling. CIN 2/C/D have two payload bytes; 3/8/9/A/B/E have three; 4 carries a three-byte SysEx continuation; 5/6/7 finish with one/two/three bytes; F carries one byte. It forwards data through `MIDIPacketListAdd` and `MIDIReceived`. This establishes standard USB-MIDI-style framing on this public path, independently of the private ring.

[Public receiver pseudocode](../analysis/decompiled-pass3/midi-0x109a0.c), evidence `ev_00fd7c1ff49dfd4016077d8558f6ed691c74180edf67bc078e988d5cc284a981`. The legacy function clamps unavailable cables and assumes complete input words; the new parser validates its inputs rather than inheriting those behaviors. Actual cable assignments and endpoint discovery still require live descriptors.

## Common plug-in engine: IPC and vendor timing confirmed

Target: `analysis/thin/VirusControl.x86_64`, SHA-256 `52bd4a7dc5fe6956b69a3a7fd79e9371cd083042947d58287f10a66d9f6dc47d`.

Opening routine `0xf135e` maps `VirusTICommMem`, accepts header versions 1 or 2, scans the 0x310-byte slots for the desired device-address field, opens the matching remote CFMessagePort, and maps the read/write buffer names and sizes. This corroborates the previous shared-memory inference with code from both binaries. Start `0xf0fe2` sends `st01`…`st09`; stop `0xf0fa6` sends `stop`. Another writer at `0xf1090` writes four-byte entries and sends a `VIRS` message (0x56495253). Typed external-call signatures in decompiled text can have reordered arguments; these identifiers and constants are the relevant observations.

[Open](../analysis/decompiled-pass3/common-0xf135e.c), [start](../analysis/decompiled-pass3/common-0xf0fe2.c), [stop](../analysis/decompiled-pass3/common-0xf0fa6.c), [writer](../analysis/decompiled-pass3/common-0xf1090.c). The routine at `0xf0eb2` writes already-framed words to the per-device ring; `0xefa42` drains separate main and slow queues. None of this code has been executed.

The packet encoder at `0xeacc8` accepts internal routes 2 and 5 and emits a header nibble plus up to three data bytes. Long messages are divided into continuation type 4 and final types 5/6/7; a slow-queue branch changes the upper nibble to 3. These are internal routing values, **not established user MIDI-channel numbers or public cable assignments**.

The timestamp path at `0xebd4c` creates three bytes `F5`, `tick & 0x7f`, `(tick >> 7) & 0x7f` and invokes the packet encoder with route 5. Combining these two observed routines derives the wire word `53 F5 lo7 hi7`. F5 is reserved in standard MIDI 1.0, so this must remain a separate vendor codec. A generic MIDI validator must not silently accept it as ordinary system-common MIDI. This path includes audio-clock conversion, offsets, realtime handling and reset scheduling that are not yet fully reconstructed; a raw 14-bit encoder alone cannot provide correct scheduling.

[Packet encoder](../analysis/decompiled-pass3/common-0xeacc8.c), evidence `ev_4c398aef4eca21d327b89cdf7375cbfb1c6ba3354375d36720c682eb92992d03`; [timestamp path](../analysis/decompiled-pass3/common-0xebd4c.c), evidence `ev_664e49deda4979e793e248c8f89a63d9c88167b7d69998be9f23707f567c4f76`.

## Evidence and next validation boundary

Complete retained records are in [MIDI evidence](../analysis/rea-midi-pass3-evidence.json) (59 records) and [common engine evidence](../analysis/rea-common-pass3-evidence.json) (24 records). Matching snapshots are `VirusTIMIDI-pass3.rea-snapshot.json` and `VirusControl-pass3.rea-snapshot.json`. All pseudocode is non-compilable analysis output. Ghidra reports 12.1.3; the REA bridge reports its reference-tested version as 12.1.4, so decompiler-specific limitations remain explicit.

Next work is descriptor-selected interface ownership and safe stream setup, then clock recovery and native device-service integration with the VST3. The adapter requires a common serial executor, real pipe owners, a future controller frame and completion drain before teardown. It does not select endpoints, issue sample-rate commands or publish a CoreAudio device. The hardware will be needed to establish exact endpoints/alternate settings, channel/PCM format, accepted rate setup, successful asynchronous transfers and cancellation, real MIDI routes, timestamp behavior and sustained audio timing. DAW automation, latency compensation, patch recall, reconnect and multi-instance arbitration follow. Offline success does not establish any of these device behaviors.
