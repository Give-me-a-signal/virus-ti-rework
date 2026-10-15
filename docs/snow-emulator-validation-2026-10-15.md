# Local Snow firmware emulation — 15 October 2026

This is an offline firmware/DSP test, **not physical Snow compatibility**.
No USB, CoreMIDI hardware endpoint, driver, updater or physical synth was opened.
Firmware, extracted data, factory-derived patches, binaries, logs and recordings
remain private; no emulator or firmware is included in the preview release.

## Configuration

M1 Pro / macOS 26.2; native arm64 Release local Gearmulator/OsTIrus host at 48 kHz,
256-frame blocks. Pinned Gearmulator commit
`897b31893b3a7c1a98aaa3909e57c785cb6ade82`, DSP56300 submodule
`7a5390e71d4de700f7877d85966d1e09056eda5b`.
Selected `DeviceModel::Snow` explicitly, using its single-DSP implementation.
The owner's unmodified 5.1.7.00 firmware container SHA-256 is
`8e3daea8552f9ecb1b7205bb5f81e7391dcf406e86af81fc7124dba00fb5cd6a`.

The local adapter maps the Snow emulator's USB outputs from channels 2–7;
the existing TI adapter uses channels 6–11. This is an emulator mapping,
not evidence of the physical macOS capture-channel order.

## Observations

| Test | Result |
| --- | --- |
| Boot Snow image and render a note | Pass; finite, nonzero audio from the Snow DSP |
| Read four parts, separate Single and Multi | Pass through the emulator's controller interface; Single/parts 524 bytes, Multi 267 |
| Initial originals | Parts initially report version 8, Single version 12. Strict restoration of the old part records fails because reloading converts them. This is an observed limitation, not a pass. |
| Legacy preflight protection | Pass: the production capture helper rejects those version-8 originals before publishing a test snapshot or editing RAM |
| Current-format baseline | Prepared only in emulated RAM from the owner's current-format Single buffer, then captured all six originals twice using production snapshot code |
| Four independent MIDI channels | Pass: each of channels 1–4 produced finite audio on its selected emulated USB1/2 return |
| Routing isolation at reduced volume | Parts 1/3: USB1 peaks 0.003491/0.003480; parts 2/4: USB2 peaks 0.003585/0.003115. Other return peaks were zero in these windows. No clipping. |
| Ten consecutive recalls | Pass using production model-bounded recall; active sound payloads and complete Multi matched after each iteration |
| Current-format restoration | Pass using production restoration; all four part buffers, Single and Multi matched exactly after final Single-mode selection |
| Legacy originals restored without conversion | Fail as expected; four sound comparisons report conversion. The preview refuses such originals before writes instead of weakening comparisons. |

The final bounded run completed in approximately 26 seconds. Initial mode-value
experiments (1, 2 and 3) all produced emulated audio; that does not establish a
physical Snow mode handshake. The preview chooses the Snow XML's declared
Multi-mode value 1, not the existing TI path's value 3. Physical confirmation is
still required. The production allowlist adds only that exact mode command,
only through the opted-in Snow test transport.

## Limits

OsTIrus runs the DSP firmware but models the microcontroller and its MIDI/dump
interface. It synthesizes replies from controller buffers and may accept buffer
selectors beyond the physical four-part model: the negative selector-4 probe
returned a cached record. Consequently these passes cannot validate USB reply
framing, driver ownership, part enumeration, stored hardware state or real recovery.
The application's own inactive-part rejection is separately unit-tested.

A six-buffer snapshot is still a model-specific implementation that must be
confirmed on hardware. The third USB return is not enabled in this preview.
Latency, long-term drift, physical unplug recovery, DAW compatibility on the
reporter's Mac and complete front-panel/global-state restoration were not tested.
The mode/USB behavior and RAM restoration must be reported separately from sound
generation. See [preview instructions](SNOW_TEST_BUILD.md) and the combined PR's
CI results before sharing a test build.
