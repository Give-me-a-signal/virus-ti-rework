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
waveforms and the derived test patch remain local and ignored by Git. Legacy generated `catalog.h`, `browser_mapping.inc` and `graph_waveforms.h` remain ignored research inputs. Public beta builds do not compile or bundle them. The compact project parameter registry retains numeric interface compatibility and protocol facts. New layouts, analytic waves and schematic graph geometry are project-authored. Optional imported appearance files stay outside distributed bundles. The graph formulas, geometry constants and protocol mappings
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
The build generates a local copy of the pinned AU wrapper with a project-owned
`IComponentHandler2` adaptation forwarding non-parameter dirty state to AU
ClassInfo property notifications. Upstream copyright headers remain intact;
`native/vst3/au_wrapper_state.cmake` records the complete adaptation.

## Separate emulator research

The local research workspace also tested Gearmulator/OsTIrus (GPL-3.0) and
dsp56300-rust (MIT). Their sources, libraries, firmware and research hosts are
not part of this hardware repository or its default build. The optional runtime
loader remains in the source; enabling an external runtime is a separate build
and licensing decision. The hardware release explicitly disables runtime bundling.

## Apple AudioUnitSDK

The AUv2 wrapper uses Apple's AudioUnitSDK 1.4.0, pinned at
`bd98b31feff57a15989fcfab4cd86dc63382b1ac`, under Apache-2.0.
Retain the upstream licence in binary distributions. Steinberg's wrapper is MIT.

Public beta archives contain native project code and dependency notices, not Access
resources. They are ad-hoc signed and unnotarized; Developer ID signing and
notarization are planned separately. Optional Access appearance imports remain local.

## TI Revive automation queue and timing findings

`native/vst3/automation_queue.h` adapts the bounded automation queue from
[TI Revive](https://github.com/derpsn/access-virus-ti-revive), revision
`21b83e6f9c05497d8331023e9ff6845361c2d36c`. Marker detection and scheduling
are independently adapted to Rework's six-channel, 48 kHz stream using the
protocol observations in its evidence E139–E145 and our issue #5.

MIT License

Copyright (c) 2026 derpsn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
