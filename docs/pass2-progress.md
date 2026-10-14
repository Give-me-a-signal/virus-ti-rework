# Second pass: native audio groundwork and plug-in boundaries

11 October 2026. Hardware remains disconnected by the user's choice. Full USB audio, MIDI and DAW integration remain the goal. This pass adds real native code and static evidence; it does not claim a working driver or plug-in.

## Native code delivered

| Module | Implemented | Validation |
| --- | --- | --- |
| `native/transport-core` | Bounded, nonallocating PCM16/24 conversion; complete-frame and capacity validation; fixed-capacity single-producer/single-consumer queue; explicit overrun/underrun; nominal integer packet pacing | arm64 normal and ASan/UBSan builds; explicit sample vectors and invalid inputs; queue wrap; 200,000-frame threaded stereo stress; every-prefix frame totals over one minute |
| `native/usb-descriptors` | Strict configuration/alternate/endpoint parser preserving raw class-specific data; speed-dependent USB2 periodic capacity; IOUSBHost descriptor-acquisition CLI | arm64 CLI; normal and sanitized parser tests; all truncations, malformed structures and 20,000 deterministic mutations; offline CLI tests |
| `native/audio-pipeline-check` | Integrates descriptor parsing, packet-capacity validation, pacing, PCM conversion and queue reblocking into 128-frame host blocks | 16 synthetic scenarios, 2,947,200 samples checked, ASan/UBSan pass |

The pipeline scenarios cover 44.1/48 kHz, assumed 1,000/8,000 USB service intervals per second, two/six channels and 16/24-bit PCM. These are test combinations, not a list of confirmed Virus modes. Descriptors and sample signals are invented fixtures. No driver or USB framework is linked into the pipeline checker.

The descriptor CLI defaults to help. `--discover` only reads registry properties. `--inspect-connected` establishes IOUSBHost device ownership and queries descriptors; it is a real compiled acquisition path but has never been run here. The SDK documents exclusive ownership even with options None. It does not seize/capture, reset, configure, select alternates or stream. Permission and ownership failures are surfaced. Device descriptor data and configuration raw bytes are retained for later analysis; actual connected behavior remains unknown.

## More of the audio engine recovered

Kernel identity remains SHA-256 `7308f591d634625acfaa0019f218af99e04cf936b4dfe18227a0bbf28a67e539`. The saved first-pass snapshot was reused. Full new evidence is `analysis/rea-kernel-pass2-evidence.json`; selected pseudocode is under `analysis/decompiled-pass2/`; latest reusable snapshot is `analysis/AccessVirusTI-pass2.rea-snapshot.json`.

| Finding | Address and evidence |
| --- | --- |
| PCM16 read maps bytes `[a,b]` to internal little-endian `[0,0,a,b]`; write emits upper16 without rounding | `0xa4e5` / `0xa564`, `ev_9e895495741c2de0bb50ccfc20caee38e9b334991a4c4ef0d040d6be3cd1e914` / `ev_0ab086ca7830b66837c5b010460473da56d6634a0b41dc04937148cd1c6d2c38` |
| Interface search matches requested sample rate/channels/resolution, then tries fallback combinations; shared code includes non-TI devices | `0xed62`, `ev_a0c4ca72df49d27988840175cca599750ab4d05cd9feb269973ff057937b453d` |
| Stream resume selects among isochronous, bulk and interrupt combinations according to initialized pipes; this alone does not prove the TI's active transfer route | `0x17496`, `ev_d738e282e944062ac439312c8089fdf3139c729ac54cd859847b90cca495a738` |
| Isochronous read/write requests draw from request-block pools and use asynchronous completion callbacks | `0x19254` / `0x1938e`, `ev_3ae74d4859c59015f5764abc4cb4155460767da69730bb88bde9a787f1e4c40e` / `ev_ada12ea2b1934aa2d239222b85ccab8e98d9f73b3c6d99f3d8530e6132f5c823` |
| Frame-pattern initialization uses 16 full-speed or 128 high-speed entries and rate-specific initial patterns | `0x1a746`, `ev_55a08a0107ad87794625361ed97a099a90aa85b5bcf6a21f0987ee08608d5109` |
| Incoming sample counts update the pattern within tolerance; out-of-range counts trigger fallback/lost-sync behavior | `0x1af08`, `ev_0efc8852bbdb96b9c54f67edaf205255ce03f9c3b62e216daddc270256bef9ff` |
| Outgoing transaction sizes derive from the current pattern and bytes-per-frame | `0x1b602`, `ev_85ee31a9b4023eb7d7f3b39e7e33f09ae02257834b2149ee6f545dfddea02208` |

At 44.1 kHz the initial full-speed table places 45 samples at indices divisible by ten and 44 elsewhere. High-speed initialization uses a separate 5/6 pattern with adjustment at indices divisible by 80. The observed tables are later updated by device input; repeating the short initial table forever is not a proven clocking algorithm. The new rational nominal schedule is deliberately a host-side design/test primitive, **not** an implementation of the recovered adaptive observer. Actual clock recovery, queue depth, timestamp conversion and recovery after missing transfers remain unfinished.

## AU, VST and MIDI conversion

The [detailed report](plugin-midi-port.md) contains source identities and exact next targets. REA now confirms both VST and AU load factories from a shared `Virus Control.bundle`. This supports one rebuilt native control engine with separate host-format adapters. A wrapper rebuild alone cannot execute the Intel common engine in a native process.

Parsed Desktop resources retain 587 parameter entries, 12 MIDI templates and 2,672 automation declarations across 16 parts. XML editor layouts and bindings survive too. Their runtime precedence over compiled definitions and their exact correspondence to host parameter IDs remain to be established; this is recoverable design data, not original implementation source.

MIDI is a separate investigation: the driver creates CoreMIDI entities, uses USB interface I/O and exposes a private control path. REA confirms private interface enumeration and directional pipe selection. The routine skips the first matching vendor-specific interface and uses the second; endpoint addresses must come from the device, not guessed constants. Public input has asynchronous reads and stall/disconnect branches, but its callback decompilation reports an unresolved jump table for some error cases. Do not treat the recovered callback as a complete behavioral specification.

A new native MIDI service and AU/VST3 front ends are plausible implementation routes. Preserving old DAW projects requires further recovery of plug-in IDs, state chunks, parameter translations and serialization versions. These are still unknown. See the detailed report for Apple/Steinberg primary documentation and the distinction between native AUv2 support and obsolete Component Manager registration.

## Next work

1. Trace common-engine parameter-definition selection, MIDI data handlers and state save/restore; establish a minimal executable parameter model from proven declarations.
2. Recover feature flags, exact alternate selection, request queue depth and frame/timestamp behavior; implement an adaptive scheduler only after the required invariants are understood.
3. With the connected device later, collect device/configuration descriptors, identify actual transport routes and confirm interface ownership. Only then add and test endpoint submission, sample-rate transitions and sustained audio.
4. Select the target DAW and build native AU/VST3 adapters over the shared engine once its lifecycle, state and audio contracts are stable.

No installer, vendor application, driver or firmware updater was executed. No hardware-facing descriptor session or audio stream was opened. Offline checks passed; native end-to-end USB audio, MIDI and plug-in operation remain unverified.
