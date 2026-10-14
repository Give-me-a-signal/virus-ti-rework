# Native Virus editor — Dark and Polar

`editor.mm` is a Cocoa `NSView` wrapped in a real VST3 `IPlugView`. `Controller::createView("editor")` returns it. It is compiled into the native arm64 plug-in. The local [preview host](../preview-host/README.md) loads the actual VST3 component/controller and attaches its view to a visible native window; it runs without audio or USB.

The current renderer preserves the recovered 907×513 logical layout and control bindings, and draws vector panels, knobs, graphs, waveforms and system text. It supports proportional **100–200%** resizing, defaults to **125%**, and offers **Virus Dark** and **Polar light** themes, plus a persistent **Original Access** artwork switch. Theme and scale are local preferences; changing them does not alter a sound or emit automation. See [modern editor notes](../../docs/modern-editor.md) for the candidate build, screenshots and current validation.

The extracted `analysis/editor-ui-catalog.json` remains the source of control positions, conditions and parameter metadata. Installer artwork remains in the development bundle for reference; the active skin does not enlarge old bitmap knobs or text.

## Working interaction

- Main Easy, Browser, OSC, Filter, LFO, Matrix, Arp, FX1, FX2 and Common tabs; Patch/Utility toggles Remote/Config. Sixteen part selectors retain per-part control state. Navigation follows actual radio-button values and conditions rather than the contradictory legacy Current Page value-list entry.
- Knob/slider vertical dragging (with a smaller horizontal contribution), Shift fine movement, Command-click or double-click default, mouse-wheel changes, radio buttons, toggles, increment/decrement and native popup menus.
- 2,672 existing VST3 catalog parameters bind by name and part. Editable mapped controls call the actual controller's `beginEdit`, `setParamNormalized`, `performEdit`, and `endEdit`. Host updates are read from the controller and redrawn by a 100ms view timer. The existing read-only softknob placeholders remain read-only.
- Explicitly declared per-part parameters missing from the automation catalog can change local editor state, including oscillator and effect algorithm selectors. They drive conditional page layouts. **These local-only values are not saved in VST3 project state and are not sent to hardware.** Hover information distinguishes them from VST3 parameters. No new host IDs were invented.
- Condition ASTs evaluate `and`, `or`, inversion, equality and inclusive inside/outside tests. Display uses the extractor's documented editor-preferred definitions. `equal` is treated as an equality alias; the unspecified-result condition stays false. Undefined conditions fail closed.
- Popup labels follow list display order and encode an explicit `value` when present, falling back to `index`. This is important for clock divisions and distortion types.
- Hit tests exclude containers and honor ancestor clipping and disabled state. A zero scroll delta does nothing. Hover shows parameter/current value and whether it is a VST3 control, local-only, or unavailable.
- Parameter-driven classic oscillator, HyperSaw, filter-response and envelope graphs draw inside theme-aware vector displays. Graph handles take precedence over enclosing masked knobs, with balanced host edit gestures and part binding fixed for the duration of each drag. See [graph evidence](graphs-evidence.md) for recovered formulas and remaining parity limits.
- The [native patch browser](browser_README.md) presents 7,164 checksum-verified patches from 62 shipped banks, with bank menus, search, category filters and keyboard selection. Double-click or Return loads the selected patch into the selected part through the existing service when connected; offline browsing applies the mapped editor fields. Version 3 project state preserves all sixteen complete patch dumps and the multi. Local-only UI parameters still have the limitation described above.

## Offline display and remaining limits

The preview stays disconnected and labels its status. For interface browsing it enables display-only page gates without writing those assumptions to a device. Part names come from verified readback when available; an empty offline instance shows “Init / Offline”. Sidebar strips now display per-part MIDI note activity with an independent, editable CC7 volume handle. Three additional USB meters show measured stereo capture peaks in dBFS. See [original meter evidence](../../docs/part-meter-evidence.md).

The existing patch/project service now supports sixteen complete RAM patches and multi recall; see [the plug-in notes](../vst3/README.md) for hardware and emulator evidence. This UI update does not add missing live parameter encodings. Explicit local-only algorithm selectors still do not change the synth or persist in project state. Bank editing, browser drag/drop, remote-template browsing, locks, original-value comparison, patch-name editing and the user arpeggiator pattern editor remain unfinished. Graphs are reconstructed editor visuals, not measured hardware output.

Tab/Shift-Tab traverse editable canvas controls; arrows edit the focused value; Escape clears focus; Space/Return activate buttons and menus. The canvas is not yet a complete VoiceOver-accessible control tree. Full original nonlinear mouse-law and menu parity remain unverified.

## 11 October verification (historical)

Candidate at that time: `native/vst3/build-modern`. Four CTest suites pass, the official Steinberg validator reports 47 passes / 0 failures, and the native preview passes dynamic loading, attachment, parameter roundtrip and teardown. Editor tests cover parameter gestures, per-part bindings, browser operations, graph handles, keyboard edits, theme isolation, aspect constraints and coordinate/drag behavior at different scales. All twelve page surfaces render in both palettes under `analysis/ui-modern/renders` (24 images).

The real preview window was checked in both themes at 100%, 125% and 150%. Larger zoom levels require corresponding screen space. A fresh API host using the candidate code restored all sixteen sounds and multi in the full OsTIrus emulator, played all sixteen parts through their assigned stereo returns and verified silent note-release tails with no added capture errors. See `analysis/ui-modern/validation.json` and `docs/modern-editor.md`.

That UI pass used the offline preview and emulator. Current physical testing is recorded in [12 October hardware validation](../../docs/hardware-matrix-2026-10-12.md). Live still needs to load the updated build. No firmware was flashed.

## 12 October update

The footer's Modern UI / Original Access menu switches renderer and browser artwork without recreating state. Dark/Polar remains a separate Modern preference. The original option reconstructs the supplied artwork; it does not execute the vendor plug-in. Original bitmap artwork scales proportionally but cannot gain the vector theme's extra detail.

REA established that the original `VUSlider` animation is note-on velocity activity, separate from its CC7 volume handle. Both renderers now receive independent activity for all sixteen MIDI channels through processor/controller messages, with a one-second full-scale linear decay chosen for this reconstruction. The original decay was one velocity unit per idle callback, whose elapsed interval is unconfirmed. Notes must pass through the plug-in's MIDI path; direct hardware keyboard activity that bypasses it is outside this display.

USB1–3 meters measure actual captured stereo audio, with a 250 ms exponential falloff and a −60 to 0 dBFS display range. They map capture channel order to the existing DAW return order. They are shared bus peaks, not per-part peaks. API readers do not consume the editor's values. Silence decays, stale capture expires after 500 ms, and disconnection clears all transient values. Meter values are excluded from project sound state and automation.

CC7 volume dragging is now enabled for all sixteen part strips, including keyboard and reset handling. Tests verify separate activity/fader pixels, correct part binding, balanced gestures and equivalent movement at 100% and 200% in both appearances.

The 12 October candidate is `native/vst3/build-original-switch`, also embedded in the dedicated `build-modern` preview. Five CTest suites and 47 VST3 validator tests pass. Current hardware evidence and USB configuration normalization are documented in the hardware report linked above.
