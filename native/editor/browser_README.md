# Browser status — preset recall update

The earlier offline-only description below records the original implementation. The current parent plug-in sends the browser's complete `sysex` NSData payload to the hardware worker for Part 1 RAM recall, or stages it until connection. Both TI checksums are checked. The bank menus can open local `.mid` and `.syx` files, including multiple singles in one SysEx file. Bank files remain local; no hardware bank is overwritten. See `../vst3/README.md` for the API and current scope.

# Native offline patch browser

`VTRPatchBrowserView` is a native Cocoa subview for the original Browser page rectangle `(192,45,705,426)`. The parent editor controls visibility and selected part. It uses the original page/menu/font resources and the XML's two pane rectangles `(19,29,671,178)` and `(19,243,671,178)`. Each pane is an eight-column, sixteen-row grid, matching the shipped background's painted 0–127 slot numbers and the packaged tutorial screenshot. The new grid clears the painted slot numbers before drawing actual patch program numbers and complete ten-character patch names, so they are not duplicated.

## Working behavior

- Two independent local-bank selectors, previous/next bank arrows, and an All Local Banks view.
- Shared case-insensitive patch-name search and category filtering. Filtering uses either stored category; selecting Off is distinct from selecting All Categories.
- Vertical scroll through large combined results, arrow-key selection and Return-to-load. Zero vertical delta, including horizontal-only/gesture-end events, makes no change.
- Single click only selects; double-click or Return calls the parent with the patch name, source bank, program, category labels and directly decoded parameter values.
- Initial banks are the first two populated libraries; the shipped empty My Patches template remains selectable. Empty search/library results are explained in the grid.
- A visible status strip says this is an offline local library. Neither this module nor its parser sends USB/MIDI, writes bank files or claims hardware recall. The parent decides how to apply the callback to its current offline part and labels partial recall.

The bank hierarchy is represented by relative folder-qualified names in a menu. Drag/drop, bank editing/saving, hardware RAM synchronization and original browser context-menu actions remain unfinished.

## Verified bank format

The package contains 62 `.mid` libraries totaling 3,760,592 bytes. All parse successfully, yielding **7,164 checksum-verified single patches**. Source directory:

`analysis/package-expanded/Additional_items.pkg/Payload/Library/Application Support/Access Music/Virus TI/Patches`

`browser_bank.cpp` parses bounded Standard MIDI Files (format 0/1), four-byte-limited VLQs, track chunks, channel/running-status events, metadata and complete/continued SysEx messages. It rejects truncated chunks, invalid status/data, missing/invalid end-of-track, unbounded payloads, trailing data and checksum failures. Valid unrecognized SysEx messages are not treated as patches.

Original `remotedevice.xml` defines `VirusC_Singledump` as 267 bytes and `VirusTI_Singledump` as 524 bytes. Both have manufacturer prefix `F0 00 20 33 01`, device ID at byte 5, command `10` hexadecimal at byte 6, bank/program at bytes 7/8, ten name characters at 249–258 and category bytes at 260/261. The trailing add7 checksum is the low seven bits of the sum from byte 5 through byte 264 or 521, respectively. Every accepted record's checksum is validated, and all interior bytes must be seven-bit data.

The XML single-dump template sets bank byte 7 to zero for its edit-buffer form, while shipped library records use storage-bank values. The parser preserves that byte rather than requiring zero or interpreting it as a USB route. Nothing is emitted back to a device.

`generate_browser_mapping.py` generates `browser_mapping.inc` from the exact original XML's direct indexed parameter entries; its header records the XML SHA-256. No bitfields, guessed offsets or decompiler source are used. Each callback value must satisfy that parameter's XML bounds. Shared-byte parameter aliases remain named separately; out-of-range mappings are omitted. This is a defensible partial offline decode, not complete program/legacy-state compatibility. The decoder rechecks message structure and checksum before exposing values.

Category labels use the XML mapped value-list's `value` fields, falling back to `index` when `value` is omitted. Names and category bytes are read from genuine files, not fabricated display entries.

## Integration

Compile `browser.mm` and `browser_bank.cpp` into the editor target. `browser_mapping.inc` is a generated C++ include. Construct with the original resource directory and the bundled `Resources/PatchBanks` directory; the parent build copies shipped files there. The selection handler receives an immutable dictionary containing `verifiedValues`, `name`, `bank`, `program`, `category1`, `category2`, `checksumVerified=true` and `offlineOnly=true`.

The handler runs on the UI thread. The parent should apply values using its existing controller begin/perform/end-edit path, separately retain supported local editor values, and report parameters that remain unavailable. Original GUI graphs are a separate module.

## Tests

Run `sh native/editor/test_browser.sh`. On arm64, ASan/UBSan passed for all 62 real banks, every byte-prefix truncation of a sample library, corrupt header and corrupt checksummed data. Cocoa tests verify all-library counts, genuine patch-name search, category filtering, single-click versus double-click callback behavior, verified decoded values, scrolling, zero-delta scroll no-op and empty directory handling. Tests render the native view to `native/editor/build/browser-render.png` for visual inspection; no host or visible application window is launched. The final render was inspected after the dark status strip and populated-bank defaults were added.
