# Pass 4: offline setup, timing, bridge and original editor

The work shifted to reconstructing the original plugin interface at the user's
request. Hardware remains disconnected. Host preview work is paused. No firmware
updater, original executable or USB streaming operation was run.

## Completed offline foundations

- `native/usb-session`: strict descriptor-derived UAC1 setup plans, rate/alternate
  snapshots, conservative rollback and callback-drain ownership. Compiled
  IOUSBHost control helpers are verified with Objective-C test doubles. The
  concrete interface/pipe-owning backend remains unfinished.
- `native/clock-sync`: bounded host/sample clock estimation, discontinuity epochs,
  capped extrapolation and private 14-bit counter unwrapping. It does not establish
  hardware clock lock or implement drift-correcting audio resampling.
- `native/device-bridge`: bounded audio and MIDI queues connecting the audio-engine
  service contract to the VST3 processor. Tests supply synthetic six-channel PCM
  and check all three stereo output buses, timestamped events, generation changes,
  underflow and overflow. Production clock mapping and device ownership are still
  required before live audio can work.

These modules have native and ASan/UBSan test coverage documented in their READMEs.
The kernel initialization/feedback analysis is retained in
`analysis/rea-kernel-pass4-evidence.json`,
`analysis/AccessVirusTI-pass4.rea-snapshot.json` and
`analysis/decompiled-pass4/`. Broad legacy reset/configuration paths were not
turned into an automatic device-start recipe.

## Interface reconstruction

The original Desktop editor supplies a 907 by 513 XML frame, 1,235 child controls
and containers, and 465 PNG resources. The original renderer's decompiled
`createView` function corroborates control-type dispatch and vertical filmstrip
frame-count derivation. This supports reproducing original artwork and geometry
without recompiling the obsolete Carbon-era editor.

`native/editor` implements a new Cocoa view embedded through VST3 IPlugView.
`analysis/editor-ui-catalog.json` preserves layout, conditions and source hashes.
The detailed boundaries between recovered evidence and reconstruction policy are
in [editor-ui-evidence.md](editor-ui-evidence.md); source compositions and the
older packaged tutorial are documented in
[the UI reference README](../analysis/ui-reference/README.md).

Reproducing page artwork does not complete patch browsing, original state recall,
dynamic custom widgets or USB synchronization. The editor's offline display policy
allows inspection while keeping hardware disconnection explicit. Parameter
editing and host interaction are new implementation code, not recovered original
C++ source. See the native editor and VST3 READMEs for current test results and
remaining behavior gaps.

The development VST3 is linked in the current user's plugin folder, pointing back
to the workspace build. No system-wide driver or plugin installer was run. The
initial preview attempt left LiveProfessor with no project/audio session; no
further host launch was performed after the user prioritized the interface.
