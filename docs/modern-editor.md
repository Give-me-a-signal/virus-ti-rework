# Modern Virus editor: Dark and Polar

The real native VST3 editor now retains the recovered Virus Control layout and draws scalable vector controls, panels, graphs and waveforms with system typography. The 16-part sidebar, page order, parameter positions, browser panes and existing host bindings remain the structural reference. The palette takes inspiration from the [Access TI2 hardware range](https://www.virus.info/virusti/overview): charcoal and red for Virus Dark; warm white, aluminium-grey and red for Polar. This is a modern interpretation, not a pixel-identical copy.

## Try it

Open `native/preview-host/build-modern/VirusTIModernPreview.app`. This dedicated preview embeds the candidate plug-in and supports offline editing and preset browsing. It does not activate audio or connect to the TI. The actual candidate is `native/vst3/build-modern/VST3/Release/Virus TI Rework Development.vst3`; that 11 October candidate was not installed in Live at the time. The installed build is now the 12 October candidate described below.

Use the footer to switch Dark / Polar and 100%, 125%, 150%, 175% or 200% zoom. Dragging the window edge also preserves the original aspect ratio. The base layout is 907×513 logical points; 125% is the default. Large scales require sufficient display space. Theme and zoom persist locally, separate from sound/project state.

Knobs retain drag, Shift fine movement, wheel adjustment and Command-click/double-click reset. Tab and Shift-Tab traverse editable controls, arrows adjust values, Space/Return activate buttons or menus and Escape clears focus. Focus has a visible outline. Hover gives the full parameter/value and identifies local-only controls. Native browser search, bank/category filters and Return/double-click loading remain available.

## 11 October verification (historical)

- Four native CTest suites pass, including editor geometry, scaled drag equivalence, keyboard edits, theme changes without automation, browser behavior and host gesture balance.
- Steinberg validator: 47 passed, 0 failed.
- Preview: actual dynamic VST3 loading, native view attachment, parameter roundtrip through the inactive processor and teardown pass.
- Real preview window: both palettes checked; zoom switched through 100%, 125% and 150%. Automated coordinate tests cover 100%, 125%, 150% and 200%.
- All twelve page surfaces render in each palette. Images are under `analysis/ui-modern/renders`; headless page images omit native footer buttons, which were checked in the real preview window.
- A fresh API host built from the candidate restored all sixteen patches and the multi in the full OsTIrus emulator. All sixteen parts sounded on their assigned stereo buses and returned to silence after note-off. Final capture errors, FIFO underruns and overruns were all zero. This uses the existing test state as its reference and does not claim a new Live `.als` reopen test.

Machine-readable results and executable hashes: `analysis/ui-modern/validation.json`. Detailed emulator results: `analysis/ui-modern/emulator/restart.json`. The test host was disconnected and stopped after verification.

## Scope and remaining limits

This is a UI update to the existing development plug-in, not completion of every original Virus Control feature. Local-only algorithm selectors remain unsaved and do not control the synth. Arpeggiator pattern editing, remote templates, locks, patch-name editing and full VoiceOver coverage remain unfinished. Updated 12 October: sidebar bars now show MIDI note activity with separate CC7 handles; USB1–3 meters show measured capture peaks in dBFS. See `docs/part-meter-evidence.md`. Existing hardware/preset functionality retains its documented limitations in `native/vst3/README.md`.

The 11 October UI pass used the preview and emulator, without physical hardware commands. The 12 October physical tests are recorded below. No stored-bank writes or firmware flashes were performed. The preview can be used while the hardware is unavailable; listening tests use the separate explicitly selected emulator backend.

## Original artwork switch and metering update, 12 October

The footer now offers **Modern UI / Original Access**. The original option restores the extracted editor and browser artwork while keeping the same part, page, patch and parameter state. Modern Dark/Polar and zoom preferences persist independently. The existing offline preview app embeds this update from `native/vst3/build-original-switch`.

The earlier verification section above describes the 11 October build. Current hardware evidence is maintained separately under `analysis/hardware-matrix-2026-10-12`; see `docs/hardware-matrix-2026-10-12.md` for completed checks and limits.

The tested 12 October build is now installed, and the preview contains the same binary. The open Live process still needs a restart to load it. Hardware results, including three USB returns, meters and recall, are in [the current report](hardware-matrix-2026-10-12.md).
