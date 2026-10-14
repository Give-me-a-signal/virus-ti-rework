# Native graph reconstruction checkpoint

The editor now draws and edits Classic oscillator, HyperSaw, LP/HP/BP/BS filter and ADSTR envelope widgets using the original XML geometry, original handle PNGs and equations recovered from the shipped x86_64 editor. These are **editor visualizations**, not a reconstructed synthesizer or independently verified acoustic filter response. No original executable or hardware was run for this work.

## Evidence

Target: `analysis/thin/VirusControl.x86_64`, SHA-256 `52bd4a7dc5fe6956b69a3a7fd79e9371cd083042947d58287f10a66d9f6dc47d`.

REA/Ghidra 12.1.3 static evidence is retained in `analysis/rea-editor-graphs-evidence.json` (112 records), with replayable snapshot `analysis/VirusControl-editor-graphs.rea-snapshot.json`. The REA target was closed after saving. Readable pseudocode is in `analysis/decompiled-graphs/`; it is decompiler output, not original source. Supplemental read-only LLVM disassembly covers `0x6967a`, for which Ghidra had no function entry.

| Widget/data | Recovered implementation | Evidence |
| --- | --- | --- |
| Classic | Constructor `0x7b550`, draw `0x7b0ac`, wave selection `0x7b030` | `ev_01faf58253a72b9046d9e8d591bca3e2c8e8ec0355fe234771309b62eb189815`, `ev_0c8d9a3252c873d45a747dfe653b4461df5fd05e8af4e04aba1dd0dfa1632b6d` |
| Spectral waves | 64 waves × 64 little-endian float samples at `0x1e5ce0`; copied as uint32 bit patterns in `graph_waveforms.h` | `ev_191c50ebeadf37b4953c872c5a07ae5959db7811fbf43782e23e36a5a28fcaa4` |
| HyperSaw | Constructor `0x7bf6c`, draw `0x7bbfa`, thresholds at `0x109160` | `ev_64b884b41d06c795f6d454af9ce460d8f8480bc441a5c7a6d5d892e6d48d1a99`, `ev_14da0e06b992af225b70df362f9c6b91315286691d6890cad83a7aaa3d7e0edd` |
| Envelope/filter templates | Constructors `0x6c600`, `0x6c250`, `0x6bea0`, `0x6baf0`, `0x6b730`; tables `0x108c18..0x108fe8` | `ev_b1500b260f646c5a75926179c4c25d46e3a31bc9d855a6ac1233f9c8548826ea` |
| Segment geometry/drawing | Logical bounds `0x682fe`, coordinate propagation `0x6967a`, segment draw `0x69c1c`, handle draw `0x6a71a` | `ev_66b4b9dbb21b4b61893098a986b069acac8ae1368a5965ac2be2f2e19826ad46`, `ev_dae3d0c81fa690eb2146522ee0c23a82e5d7bc758f16cd26bc54eff495014150`, `ev_88b70a29920629795d18cdd8f497e29ca144e0c0f3d23513416950a2ac80a566` |

Classic uses 64 display samples. For normalized Shape `s`, spectral/saw/pulse weights are `max(0,1−2s)`, `1−abs(2s−1)`, `max(0,2s−1)`. Saw sample `i` is `1−i/32`. Pulse duty is `int((PW/2+1/2)*64)`. The summed amplitude is clipped to ±1, with the original centerline closure. The spectral source is recovered static data; it is not a substitute sine wave.

HyperSaw uses vertical density lines, not a synthetic audio waveform. Thresholds are 32, 64, 80, 96, 104, 112, 120, 128; each region adds a fractional-height line, and 127 makes the final line full height. Alternating voice spacing is `(width/9−2)*DetuneSpread+2`. A misleading integer cast in decompiler output was checked against the constructor's float-store assembly.

Filter and envelope renderers share a segment model. `graph_templates.h` preserves the original node offsets, scales, modes and curvature constants. The recovered nonlinear recurrence uses base 1000; the native implementation evaluates its closed form `(1000^(c*t)−1)/(1000^c−1)` with `expm1` for numerical stability. The linear branch applies for `abs(c)<0.001`. The filter drawings therefore reproduce the legacy GUI's stylized templates rather than asserting a measured frequency response. In particular, the BS template's resonance axis has zero scale; we do not invent an effect for it.

## API and interaction

`graphs.h` exposes geometry, draw, hit, editingParameters and edits. The caller supplies an already part-resolved parameter getter and an optional original-image cache. Geometry is expressed in the same top-left coordinate system as the XML. The parent supplies backgrounds and owns VST3 begin/perform/end gestures. Reads are plain parameter values (normally 0..127, WaveSelect 0..63).

Classic maps its XML Pulsewidth binding plus same-group Shape/WaveSelect. HyperSaw maps DetuneSpread plus Density. Envelopes map Attack, Decay, Sustain, Sustain Time and Release. Filters map their XML Cutoff/Offset plus same-group Resonance. Those semantic name associations combine XML bindings with constructor axis flags and surrounding controls; the complete original binding-dispatch path has not been proven. In particular, a linked Filter 2 offset is currently displayed from its own parameter, without claiming recovered joint Filter 1 cutoff math.

The native drag implementation inverts graph coordinates, clamps to 0..127 and picks the closest integer value. It is a reconstruction: the original mouse code at `0x68044`/`0x68a7c` uses relative deltas, modifier behavior and callbacks. Original segment hit redirection, modifier fine adjustment, selected/disabled colors and pixel-for-pixel rasterization remain unverified. Native handles use a bounded 9-pixel hit radius; oscillator XY dragging uses the widget rectangle. Parent hit priority must allow these graph handles ahead of overlapping enclosing MaskedKnob rectangles.

Original seven-pixel handle images are used when supplied. The native renderer uses antialiased Cocoa paths and a translucent flat fill; the original segment renderer uses integer pixel drawing and a vertical gradient. Runtime visual parity has not been established by executing the original binary.

## Validation

Run `sh native/editor/test_graphs.sh`. The arm64 build uses `-Wall -Wextra -Werror` and AddressSanitizer/UndefinedBehaviorSanitizer. It passes finite/in-bounds geometry and inverse-drag checks across all 128 values of every mapped editable axis, all 64 spectral wave selections, saw/pulse endpoints, HyperSaw density thresholds, NaN reads/points and invalid handles/rectangles. Saturated sustain-time points are checked by restored coordinates because several values legitimately map to one clipped position. These tests caught and fixed a HyperSaw quantization boundary that could change density by one during a zero-distance drag. They verify implementation behavior and numerical safety; they are not proof of full original runtime equivalence.
