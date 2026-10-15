# Hardware bench host

This command-line development host connects the existing Virus TI CoreAudio/CoreMIDI devices to the **actual dynamically loaded development VST3**. It does not load firmware or run an updater. Read [the hardware validation report](../../docs/hardware-bench-validation.md) before use.

Build with the installed Command Line Tools (the SDK's Xcode check needs the same version override used by the other workspace targets):

```sh
rtk proxy env XCODE_VERSION=26.0 cmake -S native/hardware-bench -B native/hardware-bench/build -DCMAKE_BUILD_TYPE=Release -DXCODE_VERSION=26.0 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
rtk proxy cmake --build native/hardware-bench/build --target hardware-bench -j 6
```

Use a new output directory for each run; existing artifact names are overwritten.

Discovery uses the [shared model rules](../../docs/device-model-audit.md), including
Snow refusal before MIDI/device commands. This historical bench remains a TI
Desktop development tool; recognition of another model is not authorization to
run its write modes or evidence of compatibility. Reports retain the real audio
device name instead of replacing it with `Virus TI`.

- `--inspect OUTPUT`: device formats only.
- `--query OUTPUT`: request Single, part-1 and multi edit buffers plus the legacy global request (no global reply observed).
- `--private-query OUTPUT`: same edit-buffer queries over the hidden interface; observed replies arrive via CoreMIDI.
- `--private-plugin-notes OUTPUT`: requires initial Single mode; snapshot, temporarily enter plug-in/SEQ mode, route part0 to USB1, play three actual VST3 notes, restore mode/routing, and compare snapshots. Existing patch may be silent.
- `--private-init-notes OUTPUT`: same, with the known stock HarmncSweep RAM probe at reduced volume. This passed real hardware notes and USB return. Uses `analysis/hardware/fixtures/stock-wavetable-probe.syx`, the version12 hardware-normalized test fixture; it contains no firmware.
- `--capture OUTPUT`: eight seconds of live six-channel capture through all three VST3 stereo outputs; hardware playback buffers are zero.
- `--notes OUTPUT`: adds three low-velocity VST3 notes on channel 1; observed CoreMIDI delivery is not proof of sounding notes.
- `--cc-check OUTPUT`: snapshot two singles, send an actual VST3 cutoff automation point as ordinary CC, read back and restore.
- `--cc-check-sysex-single OUTPUT`: same automation, with a tightly scoped host adapter encoding the cutoff as Single edit-buffer SysEx. Requires the hardware to be in Single mode. This passed hardware readback and exact single restoration.

The prototype's exit status primarily indicates host/API completion. Assess `result.json`, patch readback and recordings to determine semantic success; the ordinary CC test completed but did not change a patch.

`--plugin-notes-from-single` is disabled because it stopped MIDI replies and required the user to return the panel to Single. Two recovery-only modes remain in source: `--restore-single-routing OUTPUT` applies the original standalone cable defaults and Single mode, and `--restore-multi OUTPUT SNAPSHOT` restores a checksum-validated multi RAM edit-buffer snapshot. They are not generic initialization steps and cannot guarantee restoration of unknown global state. Do not invoke them outside the matching saved bench state.

The host requires exactly one named Virus audio device, the Plugin MIDI endpoint, and the observed interleaved float32/6-channel/48-kHz capture format. It receives both Virus MIDI sources with separate SysEx assemblers. Public modes send through Plugin I/O. Private modes claim only the unclaimed vendor interface4 using IOUSBHost, leaving AURA and Apple audio in place. They send the original route2 framing, with a semantic allowlist excluding firmware commands and stored-bank writes. The private VST3 bridge ABI requires the same headers in host and plug-in and must remain within one process.

The callback maps raw CoreAudio channels3/4,5/6,1/2 to VST3 buses1,2,3, matching the original driver order. Private synth notes were observed only on USB1. MIDI is sent immediately from the control thread after each completed VST3 block.

Validate the final capture offline with `python3 tools/analyze_hardware_notes.py analysis/hardware/private-notes-final`. This verifies pitches, note-offs, exact return samples after channel mapping, wire notes, checksums and per-run snapshot restoration.

Limitations: no GUI/normal DAW integration, sample-accurate MIDI scheduling, nonzero USB playback verification, disconnect recovery, long soak test, or general patch service. The private reader observed no payload; replies use public MIDI. An OS USB abort failure can wait indefinitely for safe request completion. Read the report for earlier patch-management flag differences; per-run exact restoration does not imply all global/history state is unchanged. Snapshots and recordings are local under `analysis/hardware/`.
