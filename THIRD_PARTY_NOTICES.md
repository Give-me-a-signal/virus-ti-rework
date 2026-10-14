# Source and resource boundaries

The MIT licence covers the new implementation and project documentation, not
Access Music's software, firmware, artwork, patch data or extracted tables.
This is an independent reconstruction, not an official Access release or a
claim that the original implementation was open-sourced. It is not a clean-room
project: static reverse engineering informed the protocol and editor behavior.

## Access Music material: supplied locally by the owner

Setup reads the owner's **Virus TI Software Suite 5.1.7.00.pkg**, checks its
SHA-256 and extracts resources without running the installer or vendor code.
PNG artwork, XML layouts/parameter declarations, factory MIDI banks, display
waveforms and the derived test patch remain local and ignored by Git. Generated
`catalog.h`, `browser_mapping.inc` and `graph_waveforms.h` are also ignored.
Build bundles contain these local resources; do not upload those bundles as
MIT-only releases. The graph formulas, geometry constants and protocol mappings
in the reconstruction are documented in the engineering notes with provenance.

The README screenshots illustrate the reconstructed UI. Original Access artwork
and marks shown in those screenshots retain their respective ownership; the MIT
licence does not grant rights to that artwork. Virus TI, Virus Control, Access,
Bitwig and other product names identify compatibility, not endorsement.

## Steinberg VST3 SDK

Fetched separately from https://github.com/steinbergmedia/vst3sdk at the exact
revision in `native/vst3/sdk-lock.json`. The checked revision and its SDK
submodules carry MIT licences. Retain their licence files in any binary package;
the setup packaging command copies them into `Contents/Resources/Licenses`.
The SDK checkout is not vendored in this repository.

## Separate emulator research

The local research workspace also tested Gearmulator/OsTIrus (GPL-3.0) and
dsp56300-rust (MIT). Their sources, libraries, firmware and research hosts are
not part of this hardware repository or its default build. The optional runtime
loader remains in the source; enabling an external runtime is a separate build
and licensing decision. The hardware release explicitly disables runtime bundling.

Public binary distribution still requires a resource-distribution decision and
Developer ID signing/notarization. The public source repository plus owner-supplied
local resources is the current distribution workflow, not a notarized consumer installer.
