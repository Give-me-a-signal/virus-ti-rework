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

## Coverage wanted

| Axis | Useful new coverage |
| --- | --- |
| Apple chips | Different M-series generations; base, Pro, Max and Ultra where available. Record exact model. |
| macOS | Exact major/point/build versions from macOS 12 upward. Older releases, current updates and beta reports remain distinct. |
| Virus hardware | Additional TI Desktops; TI Keyboard/Polar; TI2 variants. Snow needs a separate topology assessment before device-write tests. |
| Hosts | Native VST3 hosts and versions beyond the recorded baseline, plus regressions on host updates. Logic/AU is outside the current VST3 release. |
| Connections | Direct USB versus specific hubs/docks/adapters, different host audio interfaces and block sizes. |

No community rows have been accepted yet. Add them by PR with a linked test issue:

| Commit | Device / firmware | Mac / chip | macOS version/build | DAW/version | USB path | Rate/block | Tested stages and outcomes | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |

Use [the testing guide](HARDWARE_TESTING.md) and [issue form](https://github.com/Give-me-a-signal/virus-ti-rework/issues/new?template=hardware-test.yml).
Do not populate unknown fields from guesses or extend one successful row into
support for an entire device family.
