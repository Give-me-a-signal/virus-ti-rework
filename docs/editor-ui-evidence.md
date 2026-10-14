# Desktop editor reconstruction evidence

The shipped `editor.xml` describes a 907 × 513 editor with 1,235 concrete controls
and containers beneath the root frame. It references the original PNG artwork,
parameter bindings and named conditions. It is sufficient to reconstruct much of
the layout directly; it does not fully specify widget painting, interaction,
dynamic browser content or compiled parameter precedence.

The generated [catalog](../analysis/editor-ui-catalog.json) preserves the source
tree and flat lookup records. Rebuild it with:

```sh
/usr/bin/python3 tools/inventory_editor_ui.py
/usr/bin/python3 tools/inventory_editor_ui.py --check
```

The check compares the saved catalog with a fresh extraction and verifies tree
references, ancestor-relative coordinates, source condition counts and bitmap
availability. It does not launch the old plug-in or validate GUI appearance.

## Source records

These are the Desktop/Polar/Keyboard resources in the unpacked common bundle.
Snow layouts are excluded.

| File | SHA-256 |
|---|---|
| editor.xml | `6b2024b49759c0a7d58316ce43a6fd5b91587864bcb7f91190c3c3323eeecb25` |
| remotedevice.xml | `817c444db3a3b089bf09b82fd0391be734057afd24ca34ca272491ccb9152944` |
| editordevice.xml | `98ceb08ac686cf075a7ee24efbd59a214b6a9d6af91117315880e196380aea9d` |

The catalog contains 465 PNG files with hashes and IHDR dimensions, 404 bitmap
declarations, 36 separate authoring prototypes, 728 parameter definitions, 163
value lists and 335 source condition records under 331 unique names. No concrete
control references a missing PNG. Prototypes are not merged into concrete nodes.

## Catalog schema

- `frame` contains the recursive control tree. `nodes` contains the same nodes
  flattened, with child IDs. IDs are stable for this exact XML traversal.
- Each node has `type`, original string `attrs`, parent-relative `rect`, derived
  `absolute_rect`, `parent_id`, `sibling_index`, preorder `draw_order`, `path`,
  original color/size `metadata` and per-attribute `sprites`.
- `parameters[set/name]` preserves original attributes plus numeric `min`, `max`,
  `default`, original `part`, value-list reference and source filename.
- `valuelists[name].entries` preserves ordered indexes, optional values, labels,
  element types and attributes. Generated labels need not match shipped tabs.
- `conditions[name].ast` preserves `and`/`or`, optional `not`, and ordered `test`
  leaves with the exact test spelling, parameter, minimum and maximum.
- The three `*_variants` dictionaries retain every source record. Lookup maps
  prefer editordevice.xml if both sources define the same name. That choice is a
  documented reconstruction policy, **not recovered compiled precedence**.
- `diagnostics` lists unresolved references and duplicate names.

## Layout and page behavior

Twenty top-level containers include the shared header at `(0,0,907,45)`, the part
panel at `(0,45,192,426)`, right border at `(897,45,10,426)`, bottom at
`(0,471,907,41)`, overlapping pages at `(192,45,705,426)`, and progress/diagnostic
overlays. Geometry is top-left based. Original child order is retained; runtime
clipping and obscured-control hit testing require separate implementation.

The frame's parameter is `Multi Parameters/Panel Part`. Its definition spans
0–15 and defaults to 0. Sixteen channel radio buttons set this parameter using
`OnValue` 0–15; channel-specific controls contain explicit `Part` values. Most
editor controls use `Part="-1"`, consistent with a selected-part binding, but
compiled inheritance behavior has not been established by XML alone.

Follow the actual radio-button `Parameter`/`OnValue` and container condition tests
for navigation:

| Section | Selection parameter/value | Page |
|---|---|---|
| Patch (0) | SelectedPage 0 | Easy |
| Patch (0) | SelectedPage 1 | Browser |
| Patch (0) | SelectedPage 2 | Osc |
| Patch (0) | SelectedPage 3 | Filter |
| Patch (0) | SelectedPage 4 | LFO |
| Patch (0) | SelectedPage 5 | Matrix |
| Patch (0) | SelectedPage 6 | Arp |
| Patch (0) | SelectedPage 7 | FX |
| Patch (0) | SelectedPage 9 | Common |
| Patch (0) | SelectedPage 11 | FX2 |
| Utility (1) | SelectedUtilityPage 0 | Remote |
| Utility (1) | SelectedUtilityPage 1 | Preferences |

All selection parameters above are under `VirusControl/`. There is a shipped
inconsistency: the `Current Page` value list labels 8 as Common and 9 as Warning,
while the actual Common tab and container use 9. The catalog preserves both.
Welcome/info, sync, recall and diagnostic containers have independent conditions.

The normal page conditions additionally require `SystemStatus/Error=0`, recall
and initial-sync progress outside 0–126, and welcome-page value 0. XML defaults for
both progress values are 127. `SystemStatus/Error` is not defined in either source
parameter file. An offline renderer may use an explicit display-state policy,
but that policy must not be mistaken for a connected/synchronized hardware state.

## Images and conditions: established versus unresolved

`KNB_Standard.png` is 43 × 5504, consistent with 128 vertically stacked 43 × 43
knob frames. Channel buttons are 21 × 42 for 21 × 21 controls. Easy-page normal
and separately named pressed images are each 62 × 42 for 62 × 21 controls. These
ratios are recorded as geometry-compatible `vertical_frame_count`; their runtime
frame-selection rules are not proved by divisibility. Do not shrink a complete
filmstrip into one control rectangle.

Bitmap fonts are supplied as strips too. For example `FNT_LCD_5x7_Rot.png` is
6 × 864; character indexing and cell metrics need runtime-code evidence or a
clearly labeled renderer assumption. Dynamic envelope, response, oscillator,
arpeggiator and browser widgets are not just background sprites.

Conditions use `equals`, `equal`, `inside` and `outside`. The catalog retains the
two equality spellings and does not assert range inclusivity without code
evidence. The lone `Parameter Visbility/Pages/Nie sichtbar` condition omits a
`result` attribute and is represented with `op: unspecified-result` rather than
inventing a default. Four delay conditions are defined in both XML files; one
material difference is Tape Delay Time, whose remote range is 2–255 while the
editor range is 2–3.

Unresolved references include two browser category-filter conditions, 21 control
parameters (browser/repository actions, patch names, remote templates, softknob
proxies and a global lock indicator), and the `SystemStatus/Error` test parameter.
They likely involve dynamically supplied state, but this is not proven. Unknown
conditions should remain explicit rather than making every conditional layer
visible at once. A full original-UI reconstruction also needs these behaviors,
widget-specific gestures, font rendering, patch browsing and connection-state
presentation; extraction alone does not establish UX parity.

## Decompiled renderer corroboration

REA/Ghidra analyzed the original common x86_64 binary (SHA-256
`52bd4a7dc5fe6956b69a3a7fd79e9371cd083042947d58287f10a66d9f6dc47d`).
Selected pseudocode is saved in `analysis/decompiled-ui/`; the complete retained
ledger is `analysis/rea-editor-ui-evidence.json` and the closed-session snapshot
is `analysis/VirusControl-editor-ui.rea-snapshot.json`. These are static observations,
not execution of the original editor. Ghidra 12.1.3 was used; the REA bridge reports
its reference verification version as 12.1.4.

- `0x8d0ec` imports the XML `Prototypes` collection into a type registry
  (`ev_bacfc681e6d308d6daa9c36ed4e2badda7d4e9dad5d558d6b6e10a9217c8c891`).
- `0x8d6a2` recognizes type names and dispatches to the typed XML attribute reader
  `0x75dcc` (`ev_966ca2e6a0a511b3b35ec2f8df52522ca97bb21c7b1d94fa0bc99a59c8a62ea9`,
  `ev_9d7c1fef901745bd9e16489fd57f58290791eef665d2a0d11d5bb17d0fe266a1`).
- `0x8d970`, identified by the binary's `createView` assertion, reads Type, Tag,
  Size, Style, colors and NormalBack, then constructs a widget by type. It divides
  bitmap height by control height to obtain the animation frame count. This
  corroborates vertical filmstrip interpretation, beyond divisibility alone
  (`ev_14dc40f56a3e24a6ab5495ac9af2f9c3396a436065996f385154579d8f9867e2`).
- The knob constructors `0x6face` and `0x372e6` pass frame count and frame height
  into the animated-control base. `0x6fd8c` explicitly recognizes KKnob,
  CAnimKnob, CKnob and CControl in the same inheritance chain
  (`ev_5e3ffefe6c35627007d2aa44860b0479955dc219e0f8c735054af5d02d0470b2`,
  `ev_a7123c00c5e94430f1649d9878bf9e91236cf96b92fed52bf75bc92e086ced5b`,
  `ev_43aeb3e052bdedee4b95a78774f4810d1d6cf6120f39c5158d66e67714ae3801`).
- `0x91126` initializes model defaults including Part=-1, Enabled=true and
  empty Parameter/Condition fields
  (`ev_9757b6d6589a7a34d8b267d15ecccc9bd90e428cc8bb88426425ecd92d454f30`).

The loading messages in `0xaeec2` prove XML resource parsing into a parameter
definition loader, but do not resolve whether its editor resource is editor.xml
or editordevice.xml. A bounded xref search for the literal editor.xml found no
direct references. Neither observation establishes the runtime layout-loading
entry point. Bitmap-font ASCII indexing, exact gesture sensitivity, parameter
inheritance and compiled condition precedence remain partly inferred. The
native reconstruction must identify its own policies rather than present them
as recovered original behavior.
