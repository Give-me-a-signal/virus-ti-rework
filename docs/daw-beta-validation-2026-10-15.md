# Packaged beta host checks — 15 October 2026

Candidate: `da5aaea7dab63cec582d2bbfb647e647276448e7` (0.2.0-beta.1).
These are bounded maintainer observations of the installed release payload, not
certification of an entire host family or a stable-release claim.

## Workstation and method

- Apple M1 Pro, native arm64, macOS 26.2 (25C56).
- Virus TI Desktop. Firmware and USB hub/adapter topology were not recorded in this pass.
- Setup installed the packaged AU and VST3 in the current user's folders and backed
  up the previous Development VST3. No separately installed VST3 was required by AU.
- Separate test projects and private RAM audition sounds were used. No firmware
  updates, stored-bank writes or forced host crashes were performed.
- USB capture stays at 48 kHz. Return tests routed a low-velocity note to USB1,
  USB2 and USB3 in turn, inspecting capture and processor-output telemetry.
  This establishes audio reaching the plug-in's three stereo outputs; it does
  **not** establish every host's auxiliary-track routing or physical monitoring.

## Results

| Host | Format | Rate | Observed result | Still to test |
| --- | --- | --- | --- | --- |
| Logic Pro 12.4 | AUv2 | 48 kHz | Loads native editor; Dark/Polar switching; scale selector responds at 100%/150%; three internal stereo returns; a real MIDI-region loop reaches the TI and instrument/Stereo Out meters | Project reopen/recovery result being recorded; host auxiliary routing and automation lanes |
| Bitwig Studio 5.2.7 | VST3 | 48 kHz | Loads packaged VST3; three internal stereo returns; normal-exit helper reports restored | New beta project reopen, host MIDI clip and auxiliary routing; full editor interaction blocked by computer-control access to the custom host window |
| Ableton Live 12.4.1 Suite | VST3 | 44.1 kHz | Loads editor; computer-keyboard MIDI reaches TI; three internal stereo returns; manual Disconnect restores all 16 buffers and Single mode | Save/reopen and export deferred: Live is not authorized on this workstation; host auxiliary routing and automation lanes |
| LiveProfessor 2025.1.0 | AUv2 | 48 kHz / 512 frames | Loads embedded AU; Polar switch; three internal stereo returns; saved project reopens and reconnects; normal-exit restoration | Host MIDI-input routing, external output patching and automation |

Every completed three-return run recorded **zero capture errors, FIFO overruns
and FIFO underruns**. The corresponding active return peaked near 0.0271 in USB
capture, with nonzero values in both processor output channels of each stereo pair.
These short runs do not establish long-session or high-load reliability.

Logic's MIDI loop advanced the hardware MIDI-send counter by six messages during
a controlled playback check with no API-generated notes. Its instrument and
Stereo Out peak meters showed approximately −29 dBFS. Ableton's keyboard test
also advanced the send counter by a note-on/note-off pair.

LiveProfessor project recall preserved all sixteen sounds and routing. Parameter
values differed by at most 2.440505142420335e-8 after the AU float round trip;
there were no differences above 1e-6. Part 1 differed only at metadata byte 10
and checksums 265/522; other parts were byte-identical. The multi differed at
metadata byte 10 and checksum 265. Normal host exit independently restored RAM.

## Observations and limits

Loading AU and VST3 together did not allow duplicate hardware ownership. A
LiveProfessor AU connect attempt while Live owned the TI was rejected with
“No device state changed.” Logic also rejected a connect while another instance
owned the device.

A shared audio-device rate change from 44.1 to 48 kHz during the first
LiveProfessor pass disconnected the plug-in and restored the original RAM.
The three-return test was repeated successfully after reconnecting at 48 kHz.
Stop playback and reconnect after host/device reconfiguration; seamless
rate-change continuation is not established by these results.

The 150% Logic editor window extended beyond the visible screen at its existing
position. The scale menu remained accessible and allowed returning to 100%.
This pass does not claim that large editor scales fit every screen position.

Computer-control timeouts and Bitwig's inaccessible custom save dialog limited
some UI steps. They are recorded as incomplete tests, not plug-in crashes.
Private raw projects, patch dumps and telemetry remain outside the public source
repository. See [the compatibility matrix](COMPATIBILITY.md) for other test runs.
