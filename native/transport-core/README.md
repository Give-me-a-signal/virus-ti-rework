# Offline transport-core candidate

This standalone C++17 module reconstructs bounded operations from static
driver evidence and adds offline host-side audio buffering and nominal pacing. It is **not a working Virus TI driver**. It performs no USB I/O,
device enumeration, firmware changes, audio streaming, installation, or deployment.

Build and run on this Apple Silicon host:

```sh
sh native/transport-core/build_and_test.sh
```

The script builds an ARM64 test executable on macOS, runs explicit vector and
validation tests, then builds and runs with AddressSanitizer and UndefinedBehaviorSanitizer
when compilation supports them. `TRANSPORT_BUILD_DIR` can change the output folder;
`CXX` selects a compiler executable. Only the C++ standard library is required.

## Recovered operations and bounds

* `make_sample_frequency_request` serializes an eight-byte USB setup packet and
  three-byte payload. Evidence: REA
  `ev_93c97d7fc814a2344ebae8a1de3e8e999588a8969ad0284f9d290a868f4acad6`,
  `PGKernelDeviceVirus::setFrequency`, address `0x1ed52`: request type `0x22`
  (host-to-device/class/endpoint), request `1`, value `0x0100`, caller endpoint
  in index, length `3`, and frequency in little-endian 24-bit encoding.
  The builder restricts candidate frequencies to 44100 and 48000 based on
  `findInterfacesInConfig` defaults/clamping, and rejects endpoint zero or reserved
  address bits. These checks are reconstruction policy; they do not establish that
  either rate is supported in every device mode. Direction bit 7 is preserved.
* `pcm_from_24_lsb` maps packed bytes `[a,b,c]` to the 32-bit bit pattern
  `[0,a,b,c]` in little-endian byte order (left-aligned signed PCM). Evidence:
  `ev_aa56aaea6fb79accca77410bd6a0d8eafecc68defde450bf4063346120ecfdbe`,
  `pcmFrom24LSB`, address `0xa5cc`.
* `pcm_to_24_lsb` emits the upper 24 bits as three little-endian bytes, discarding
  the low eight bits without rounding. Evidence:
  `ev_550f85e629d1ca56557639b887b3ae878a349780f45cb662eecccc816bf5291b`,
  `pcmTo24LSB`, address `0xa621`.
* `pcm_from_16_lsb` maps packed bytes `[a,b]` to the 32-bit bit pattern
  `[0,0,a,b]` (left-aligned signed PCM). Evidence:
  `ev_9e895495741c2de0bb50ccfc20caee38e9b334991a4c4ef0d040d6be3cd1e914`,
  `pcmFrom16LSB`, address `0xa4e5`.
* `pcm_to_16_lsb` emits the upper 16 bits without rounding. Evidence:
  `ev_0ab086ca7830b66837c5b010460473da56d6634a0b41dc04937148cd1c6d2c38`,
  `pcmTo16LSB`, address `0xa564`.

All shifts operate on unsigned values. Signed reconstruction uses an in-range
64-bit intermediate. PCM functions reject incomplete interleaved frames and invalid
channel counts rather than silently consuming a partial sample. These are chosen
safe API contracts, not an assertion that vendor routines validate the same inputs.

The recovered driver also has defaults of six input channels, two output channels,
and 16-bit samples. The presence of the 24-bit converters does **not** establish
that the Virus TI always streams 24-bit audio or that this module determines the
active USB alternate setting. Endpoint discovery, device mode negotiation, USB
transfer submission/completion, synchronization, and live hardware validation remain
absent. The new queue is a host-side primitive; it is not wired to any USB endpoint.

Tests use explicit positive/negative PCM vectors, both signed extremes, low-bit
truncation, exact 44100/48000 setup packets, invalid rates/endpoints, partial frames,
and channel-size overflow. Tests do not constitute hardware protocol validation.

Validation on 2026-10-11: the normal build and the combined AddressSanitizer /
UndefinedBehaviorSanitizer build both passed. `file` identified both test executables
as `Mach-O 64-bit executable arm64`. No live device was accessed by these tests.

## Bounded audio API

`pcm_decode` and `pcm_encode` accept a format, raw pointer/count, channel count, and
caller-supplied output pointer/capacity. They return `BufferStatus`, never allocate,
and never throw. All validation precedes any destination write: unsupported formats,
zero/overflowing channel counts, incomplete interleaved frames, size overflow,
insufficient output capacity, null nonempty buffers, and overlapping input/output
ranges are rejected without partial output. Packed byte buffers may be unaligned;
`int32_t` buffers use ordinary C++ alignment. The caller must supply valid accessible
storage matching its declared sizes. Zero-length buffers may be null. Allocating
vector wrappers remain conveniences for offline use; use bounded calls in audio I/O.

`AudioFrameQueue` owns its storage, allocating at construction only. Its capacity in
frames must be a power of two; each frame contains the configured channel count.
Exactly one producer uses `push` and one consumer uses `pop`. Acquire/release atomics
publish samples and retire consumed slots; producer/consumer counters occupy separate
cache lines, and the build requires always-lock-free index atomics. Unsigned indices
and power-of-two addressing preserve wrap behavior. Each call transfers the entire
requested frame count or nothing. Overrun and underrun are explicit return statuses;
underrun leaves the output untouched, allowing a future host to choose silence,
resynchronization, or another policy. No synchronization reset is exposed while the
queue is running, and destruction requires the producer/consumer to have stopped.

## Nominal packet pacing is a design fixture

`validate_audio_packet_config` checks caller-specified rate, interval count, channels,
format, and maximum packet bytes. It derives bytes per interleaved frame and the
largest nominal packet, rejecting insufficient capacity and arithmetic overflow.
The caller will eventually obtain endpoint capacity from actual descriptors and the
selected alternate setting. No endpoint addresses or device alternate settings are
hardcoded. Rates 44100/48000 and 1000/8000 intervals per second are supported for
**offline calculations only**; this does not establish the device's active bus mode,
service interval, or ability to use a particular channel/format combination.

`NominalPacketSchedule` distributes integral frames using rational floor/carry
arithmetic. Every prefix sums to `floor(interval_count * sample_rate / interval_rate)`;
there is no floating-point drift. For example, 44.1 kHz over 1000 intervals emits nine
44-frame packets then one 45-frame packet. The schedule is **new host-side design,
not a reconstruction of the driver's device clock/feedback scheduler**, and packet
capacity here bounds only this nominal schedule, not future feedback corrections.

In particular, additional static evidence shows the original
`PGFramePatternObserver::init` at `0x1a746` uses 16 full-speed or 128 high-speed
entries, with a different initial phase/pattern: at 44.1 kHz the full-speed pattern
uses 45 frames at indices divisible by ten, otherwise 44; the high-speed pattern uses
`5 + (index & 1) + (index % 80 == 0)`.
Evidence: `ev_55a08a0107ad87794625361ed97a099a90aa85b5bcf6a21f0987ee08608d5109`.
`onRtsFrame` at `0x1af08` adapts its pattern from observed input sample counts subject
to tolerances, falls back and marks lost synchronization on failure.
Evidence: `ev_0efc8852bbdb96b9c54f67edaf205255ce03f9c3b62e216daddc270256bef9ff`.
This adaptive observer, actual feedback transport, device clock recovery, and
asynchronous USB transfer lifetime management are not implemented by this module.

## Extended offline verification

The test suite includes independent signed PCM16 and PCM24 vectors, both signed
extremes and low-bit truncation, unaligned packed buffers, guard values, overlap and
capacity rejection without writes, incomplete frames, null inputs, arithmetic
overflow, exact packet capacity checks, one-minute prefix-by-prefix schedule checks
for all four rate/interval combinations, queue wrap/overrun/underrun behavior, and a
two-thread ordered stereo transfer of 200,000 frames. The normal ARM64 build and
combined AddressSanitizer/UndefinedBehaviorSanitizer build passed on 2026-10-11.
Sanitizers and stress tests validate these host primitives; they do not establish
real-time deadlines, USB clock correctness, or device compatibility.
