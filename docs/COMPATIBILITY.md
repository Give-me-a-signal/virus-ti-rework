# Compatibility and test coverage

This is a record of bounded observations, not a universal support list.

- **Recorded project test:** described in an engineering report with its scope.
- **Community reported:** reproducible configuration and results submitted in an issue;
  not independently reproduced by a maintainer unless explicitly marked.
- **Not tested:** no result for that combination. It does not mean incompatible.
- **Failed / regression:** identify the failing stage and link evidence rather than
  labelling every use of that device unsupported.

## Existing baseline

| Device | Mac / OS | Host / audio | Evidence and scope |
| --- | --- | --- | --- |
| Virus TI Desktop | Native Apple silicon; exact chip and firmware not enumerated in this summary | Bitwig 5.2.7; 44.1 kHz / 256 | [Recorded project test](bitwig-validation-2026-10-12.md): playback, sixteen parts, save/reopen and routing; [normal-quit recovery](quit-restoration-2026-10-12.md) |
| Same project TI Desktop | Same test workstation; do not infer other Macs | Native test host; 48 kHz / 256, 44.1 kHz / 64, 96 kHz / 512 | [Hardware matrix](hardware-matrix-2026-10-12.md): scope differs by rate; includes three returns and bounded counter/audio tests |
| No device required | Apple silicon / macOS 26.2 | Local build and offline tests | [14 October source packaging verification](repository-verification.json); a build result is not a new connected-hardware test |

## Packaged 0.2.0-beta.1 host checks

On 15 October 2026, the packaged beta was smoke-tested on an **M1 Pro / macOS
26.2 (25C56) / TI Desktop** with Logic Pro 12.4 (AU), Bitwig 5.2.7 (VST3),
Ableton Live 12.4.1 (VST3) and LiveProfessor 2025.1.0 (AU). All four processed
real TI audio on three internal stereo returns. Host MIDI, save/reopen, auxiliary
routing and recovery coverage differ by host: read the [bounded test report](daw-beta-validation-2026-10-15.md)
before treating a stage as verified. Ableton save/reopen is deferred because
Live is not authorized on this test workstation.

## Coverage wanted

| Axis | Useful new coverage |
| --- | --- |
| Apple chips | Different M-series generations; base, Pro, Max and Ultra where available. Record exact model. |
| macOS | Exact major/point/build versions from macOS 12 upward. Older releases, current updates and beta reports remain distinct. |
| Virus hardware | Additional TI Desktops; TI Keyboard/Polar; TI2 variants. Snow has a four-part test path in PR #9; physical playback/recall/recovery remain unverified. |
| Hosts | Native VST3 hosts and versions beyond the recorded baseline, plus regressions on host updates. AUv2 coverage is recorded separately for the downloadable beta; GarageBand remains untested. |
| Connections | Direct USB versus specific hubs/docks/adapters, different host audio interfaces and block sizes. |

Community reports below have not been independently reproduced. Unknown fields
remain explicit; they are useful evidence of a failure, not complete support tests.

| Commit | Device / firmware | Mac / chip | macOS version/build | DAW/version | USB path | Rate/block | Tested stages and outcomes | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Release/commit not supplied | TI Snow / firmware not supplied | Apple Silicon Mac / model and chip not supplied | Sonoma / point version and build not supplied | Ableton Live 12 / point version not supplied; VST3 | Genesys Logic hub, then direct; same failure | macOS audio reports 48 kHz, six inputs/two outputs; host block not supplied | **Reported failure:** Connect says audio absent although macOS lists `Virus TI Snow`; MIDI device not visible. Playback/recall/restoration not tested. | [Issue #8](https://github.com/Give-me-a-signal/virus-ti-rework/issues/8) |

The [model audit](device-model-audit.md) separates discovery names from transport
support. Current PR #9 builds recognize Snow and let **Connect TI** use its
four-part path, while displaying an unverified status. Snapshot/format, ownership,
MIDI/USB identity and helper-readiness checks remain mandatory before RAM edits.
There is no new physical Snow compatibility result. TI/TI2 Keyboard/Polar and TI2 Desktop also remain
unverified on our build.

Use [the testing guide](HARDWARE_TESTING.md) and [issue form](https://github.com/Give-me-a-signal/virus-ti-rework/issues/new?template=hardware-test.yml).
Do not populate unknown fields from guesses or extend one successful row into
support for an entire device family.
