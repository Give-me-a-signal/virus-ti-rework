# Interactive native interface, graphs and local patch library

The actual rebuilt arm64 VST3 is open in `VirusTIInterface.app`, a small Cocoa
host that loads a stable bundled plugin copy. Its processor stays inactive;
there is no audio callback, USB access or hardware recall. Workspace builds do
not overwrite the running snapshot. See [the host README](../native/preview-host/README.md)
for the guarded refresh procedure.

## Implemented

- Classic oscillator graphs use the original 64 × 64 display-wave table and
  recovered spectral/saw/pulse blending. HyperSaw uses the recovered density
  thresholds and detune-spacing display. Filter and envelope curves use the
  original segment tables and recovered base-1000 curve equation.
- Graph handles update the appropriate part's VST3 parameters with balanced
  begin/perform/end gestures. Hit testing prioritizes these handles over their
  enclosing knob artwork. Local-only parameters stay explicitly separate from
  host-saved state. Original gesture and rasterization equivalence is incomplete.
- The two-pane browser reads all 62 supplied MIDI banks: 7,164 single patches
  with verified checksums. Bank menus, search, categories, scrolling and keyboard
  selection work. Single click selects; double-click or Return performs partial
  local recall through verified XML parameter mappings. Fields outside this
  partial decode start from XML defaults, avoiding stale values from another
  patch. The footer identifies partial recall and hardware disconnection.

Graph evidence and limits are in [graphs-evidence.md](../native/editor/graphs-evidence.md).
REA records, pseudocode and the closed-session snapshot are retained under
`analysis/rea-editor-graphs-evidence.json`, `analysis/decompiled-graphs/` and
`analysis/VirusControl-editor-graphs.rea-snapshot.json`. Browser format provenance
and parser limits are in [browser_README.md](../native/editor/browser_README.md).

## Verification and live check

Release and ASan/UBSan processor/editor tests pass; the official VST3 validator
reports 47 passes and zero failures. Dedicated sanitizer tests cover graph-axis
sweeps, all 64 display waves, density boundaries, inverse dragging, malformed
bank files, genuine-bank parsing and browser interactions. The real view test
checks graph-handle event routing and per-part partial recall. The preview host
tests dynamic loading, view attachment, edit propagation and teardown.

The visible native window was opened and checked. A genuine shipped preset was
double-clicked into Part 1; its name and partial-recall status appeared. Switching
to Filter showed its parameters, and dragging the filter handle changed the
curve, cutoff value and associated knob together. The final build was then
refreshed and left open on Browser. This is a live reconstruction check, not a
runtime comparison with the original Intel editor or a connected Virus.

The final binary hash and exact coverage are in
`native/vst3/verification.json`. Twelve updated native renders are saved in
`analysis/native-editor-preview/`.

## Remaining work

Audio and real USB service integration, hardware patch transfer, complete legacy
state compatibility, bank editing/saving, browser drag/drop, arpeggiator pattern
editing, remote templates and exact original UI behavior remain unfinished.
Local-only values and patch labels are not persisted across editor sessions.
