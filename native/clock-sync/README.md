# Clock synchronization groundwork

This is an offline, fixed-memory timing module for the future device service. It
is **new code, not Access's recovered PLL**, and has not seen Virus hardware.

`ClockEstimator` fits cumulative sample frames against monotonic host nanoseconds
using up to 64 anchors. It distinguishes sample anchors from USB completion
observations, decimates densely spaced observations, rejects implausible rates
and timing outliers, resets on counter reversal or a long observation gap, and
refuses mappings until it has enough history. Frame/host mappings and the bridge
between two sample domains have a bounded extrapolation horizon. All operations
are bounded, allocation-free and intended for one serialized executor.

Use `include/virus/clock_sync.hpp` and compile `src/clock_sync.cpp`. The public
`HostTimeNs`, `SampleFrame` and `PrivateTick` types deliberately separate clock
domains. They must not be filled with USB frame numbers interchangeably.

## What the estimates mean

The IOUSBHost adapter currently reports transaction completion times. Repeated or
delayed completions do not identify precise audio sample time. A fit can estimate
long-term rate, but cannot discover an unknown constant interrupt delay, hardware
latency or a DAW's presentation offset. `uncertainty_ns` describes observed fit
residuals with a configured floor; it is neither a statistical confidence bound
nor a complete latency error bound. `bridge_frame` preserves both fits' empirical
uncertainty, but does not correct drift or resample audio.

Supply a continuous sample counter within one stream epoch. Received-frame totals
after discarded/missing packets do not necessarily describe device sample time.
Call `reset()` after stream restarts, packet discontinuities, sample-rate changes,
or a changed timestamp interpretation. Do not silently join pre- and post-restart
sample counters. The implementation automatically reanchors on decreasing counters
or timestamps and gaps above the configured threshold. Duplicate host timestamps
and observations too closely spaced for a useful window do not change the fit.

Defaults target diagnostic observations at 44.1/48 kHz: 250 ms minimum fit span,
±2000 ppm rate limit, 3 ms outlier limit and 100 ms extrapolation. These are
engineering defaults, **not measured Virus limits**. The fit does not supply a
production resampling policy, playback feedback schedule or a hardware lock flag.

## Private MIDI timestamp groundwork

Saved common-binary evidence `analysis/decompiled-pass3/common-0xebd4c.c` emits
`F5 lo7 hi7` through private route 5; the companion encoder produces the observed
`53 F5 lo7 hi7` word. This proves a 14-bit transmitted value, not its units or
relationship to DAW samples. The same recovered path contains audio-clock
conversion and special offsets/reset logic that this module does not reproduce.

`Raw14Unwrapper` extends forward modular values only when they fall inside a
caller-justified maximum step below half the 16384-tick modulus. A backwards host
time, excessive gap or ambiguous modular distance poisons the epoch until explicit
reset. Its host gap limit must be justified independently; absent knowledge of tick
frequency, a small modular delta cannot disprove that a whole wrap was missed.

`private_deadline` is disabled unless the caller supplies an explicitly verified
rational ticks-per-sample calibration. It checks arithmetic with a 128-bit
intermediate, rounds toward the future, rejects past/overflowing deadlines, and
limits future distance to less than half the wire modulus. No such hardware
calibration currently exists, so this API is scheduling groundwork only.

## Verification

Run `sh native/clock-sync/build_and_test.sh` from the workspace. Native arm64 and
AddressSanitizer/UndefinedBehaviorSanitizer builds pass:

- Fifteen 120-second synthetic traces at 44.1/48/96 kHz with ±1200 ppm skew and
  bounded jitter; rate estimate error stayed below the asserted 110 ppm limit.
- Dense 1 kHz completion decimation, unavailable mappings, outlier rejection,
  discontinuity epochs, bridge behavior and extrapolation limits.
- Absolute counters near `uint64_t` limits, without converting their absolute
  origins to floating point, plus checked mapping/deadline overflow.
- Every 14-bit starting value across a wrap, backwards/ambiguous observations,
  explicit resets, and one million successive ticks.

Hardware is needed to establish timestamp semantics, true sample-rate drift,
private tick frequency/reset behavior, feedback phase and acceptable queue depth.
No USB device was opened and no private timing messages were transmitted.
