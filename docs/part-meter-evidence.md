# Original part-strip meter evidence

Static investigation, 12 October 2026. The original Virus Control part strip combines a **CC7 volume fader** with a **MIDI note-velocity activity display**. The animated strip is not established as a per-part audio peak meter. Recreating its original behavior does not require a new hardware telemetry command.

## Identity and method

Target: `analysis/thin/VirusControl.x86_64`, SHA-256 `52bd4a7dc5fe6956b69a3a7fd79e9371cd083042947d58287f10a66d9f6dc47d`, extracted from the supplied Access installer. REA/Ghidra 12.1.3 reused `analysis/VirusControl-editor-ui.rea-snapshot.json`. New retained observations are in `analysis/rea-part-meter-evidence.json`. This investigation did not execute the legacy plug-in or send anything to hardware.

## Findings

| Finding | Evidence |
|---|---|
| All 16 `VUSlider` nodes bind `Internal MIDI Standard Parameters/07 Volume`, parts 0–15. | Shipped `editor.xml`, lines 1683, 1758, …, 2808; resource background `BG_VU_Meter.png`, handle `FDR_Handle_Small.png`. |
| The compiled engine additionally creates a per-part `VirusControl/MIDI Indicator` parameter, range 0–127, default 0. `ShowMIDIActivityMeters` defaults enabled. | `0xb5a06`, `ev_78e9ab2d19f1ecd80b3ef7e574e19e3bd3447b6ccb52a9d64a684174ebb393f6`; preference initialization `0x54a1a`, `ev_9f4ffcdabf975fe8cf13d1555fc7a06752b00fa1896e4063f599b7eb33887eac`. |
| MIDI note-on updates activity by channel, taking the maximum of its velocity and the existing value. | `0xab030`, `ev_7c81fcb8f6076ad6dd21f25032c2cc34dba4ad6471ee02632d581ce041700a0a`. |
| Each idle update publishes the 16 activity values, then subtracts one from every positive stored value. | `0xb2a3e`, `ev_52b12075b6ed2e4e50b4fbb69e97211f56423994a636548eadd3cb8697850800`. |
| The widget has separate volume and activity parameter pointers and normalized values. | `0x9481c`, `ev_0478d036ac95fa47cefa7d1df0f8515dd2745a6018e6edb89a6a2c69cb8b88a1`; `0x9483e`, `ev_f2d9c064a24ba08fe3e6be52b5b94e1a0c33279ee4b8ac037e835d2a24632da9`; signal handler `0x93e54`, `ev_f299c100998012100931a73e7b02efa322a7a748639e1c8fcbba67146f2399bf`. |
| Activity chooses the background bitmap frame. Volume positions the separate fader handle. | Widget draw `0x94328`, `ev_5dba47f804d1e29b3731afe8f17b6352aa08f78983250a4121d748012f75ba63`. |

The note-on branch requires `(status & 0xf0) == 0x90`, message length 3, activity enabled, and the caller's event-source argument equal to 2. It indexes by `status & 15` and stores `max(old, message[2])`. Note-off does not trigger the branch. A zero-velocity note-on cannot raise the nonnegative value. The activity is not held high for the duration of a sustained note: it decays independently after the attack.

The per-part activity array is at engine offset `0xccd68` (16 integers); its parameter pointer is at `0xccd60`. The widget stores the volume parameter at `+0x100`, its normalized handle position at `+0xc4`, activity parameter at `+0x1f8`, and normalized activity at `+0x200`. Drawing selects `min(frameCount - 1, int(frameCount * activity))`, optionally inverted, as the background's vertical frame offset. The fader handle is drawn separately from the volume value.

The final parameter-binding sequence at `0x936c4–0x937e0` lies outside Ghidra's recovered function bodies. Apple's `otool -tvV` independently decodes the shipped bytes: it loads widget vtable slot `+0x2f8` at `0x93769`, constructs the literal `VirusControl/MIDI Indicator` at `0x93780`, looks it up via `0xa571e` at `0x9379a`, and calls that slot at `0x937a8`. The KVUSlider vtable at `0x1ab970` maps `+0x2f8` to `0x9484c`, which stores the pointer at `+0x1f8` (`ev_1a2d4ec49ab4ec7706e3ff9b113337812ba44123f01ad19ac71c26149f68f577`). This binding is assembly-derived; it is not a successfully decompiled REA function. REA reports those individual instruction addresses as undecodable because its Listing has not defined them.

## Reconstruction guidance and limits

- Display **MIDI activity** independently for each of the 16 parts. Keep the CC7 volume marker separate. Do not label the activity bar in dB or represent it as proof that sound reached the USB return.
- Measure actual audio peak/RMS from each of the three stereo USB returns and label those as USB audio meters. Multiple parts may share a bus, so its mixed level cannot be assigned truthfully to an individual part.
- Reset transient activity on disconnect/reconnect and keep it outside project parameter/state persistence.
- The original decay is one velocity unit per idle call. Its real-time interval is **not established** by this investigation: `0x527d6` calls the decay function through a virtual idle path. Any chosen elapsed-time decay in the reconstruction is a documented design choice, not measured original timing.
- No per-part audio telemetry packet, SysEx poll, private request or safe query was established. None is needed to reproduce the observed original MIDI-activity mechanism. This bounded investigation does not prove that no separate audio telemetry exists elsewhere.

## Other unfinished reverse-engineering/reconstruction work

Current implementation documents, rather than older progress reports, establish these priorities:

1. **Live parameter coverage:** editor-local algorithm selectors and other unbound controls still do not update hardware or project parameters. Full patch dumps preserve loaded sounds, but this does not make every visible control functional. Complete mappings and test independent part edits first.
2. **Original editor actions:** user arpeggiator pattern editor, remote-template browsing, locks, compare-to-original, patch-name editing, bank editing/saving and browser drag/drop remain unfinished.
3. **State compatibility:** the reconstruction's versioned state works independently of original Access vendor chunks. Legacy Access project migration and exact vendor state compatibility remain separate RE work. Actual Live `.als` save/reopen remains blocked by the previously recorded licence limitation; component-state API testing is not equivalent to that DAW workflow.
4. **Performance and transport completion:** pitch bend, channel/poly pressure and program-change forwarding remain unfinished alongside parameter encodings outside ordinary CCs. Fade-before-disconnect, comprehensive latency/transport behavior, clean installation/signing and wider DAW/host validation are engineering work still recorded in the plug-in notes. A complete physical hardware matrix must distinguish tests performed now from prior emulator-only coverage.

Sources: `native/editor/README.md`, `native/vst3/README.md`, `docs/modern-editor.md`. These are feature gaps, not claims that already implemented audio, preset loading or 16-part state support must be started again.
