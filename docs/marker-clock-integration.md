# Marker timing integration — implementation and validation boundary

This is a **draft implementation**, not a validated timing release. Normal
plug-in operation retains immediate MIDI delivery. The experimental host must
explicitly set `VTR_ENABLE_MARKER_TIMING=1`; do not set that on a production DAW.
These build targets do not install plug-ins automatically or replace the published beta.
Hardware acceptance currently fails one AU case (48 kHz/512 frames, three FIFO
underruns and one missing recorded onset). See
[the validation report](ownership-timing-validation-2026-10-15.md). The later fault
guard corrects false locked status, not the underlying underrun.

For disposable Logic/Bitwig tests, a separate build directory may explicitly set
`-DVTR_TIMING_DAW_TEST_BUILD=ON`. This opts only those test bundles into timing
without changing the GUI app's environment or the ordinary build default.
`VTR_ENABLE_MARKER_TIMING=0` still overrides it. The status API identifies the
test build; the release packager refuses it. Back up installed bundles before
temporarily using it and restore them after the test. Never distribute that build.

## Design

The six-channel capture stream remains 48 kHz. Before channel remapping, a
single capture-thread detector looks for an outstanding calibration ping's
isolated right/left marker on raw channels 5/6, following one second of silence.
It retains the candidate and 64-frame trailing-silence check across callbacks.
Ordinary impulses, other-channel activity, cancelled generations and incomplete
patterns cannot establish calibration. Confirmed marker frames are masked from
the audio resampler; initialization/recall remains muted until preparation ends.

The private timing word is encoded separately from standard USB MIDI. A dedicated
internal allowlist permits only FE, F2 song position, F8 and the exact evidenced
`73 00 22 00` calibration ping. It does not permit arbitrary host SysEx or extend
firmware/stored-bank support. There is one outstanding ping, five seconds per
response, at most three attempts, and silence is required before each attempt.
No USB reset is used.

The device timeline starts 4,096 frames behind the newest captured frame, with
2,048 frames of transmission lead. The capture margin is
`max(2 * ceil(maximum_host_block * 48000 / host_rate), 1024)` plus 32 frames
of resampler look-ahead. Reported host latency uses the same device-frame model.
USB writes batch at most 32 timestamp/message pairs, validating every message before
submission and preserving packet order. MIDI offsets derive from the actual fractional resampler read position/rate, so
audio and MIDI do not apply unrelated drift corrections. The private 14-bit
deadline must remain in the unambiguous future horizon (<8,192 frames).

The audio thread copies fixed-size events into a bounded queue and publishes a
whole block before the worker drains it. The worker orders offsets, controls
and notes before USB submission. Latest-value automation is limited to 250/s
with burst 64. The last 256 queue slots are reserved for note-offs/panic controls;
an exhausted safety reserve requests panic and invalidates the old generation.
Transport discontinuities invalidate pending events without sending another
calibration ping. Audio callbacks perform no USB I/O, allocation or mutex waits.

`status` keeps its existing fields and adds `device_lease_held`, `timing_enabled`,
`timing_status`, `timing_marker_frame`, `timing_overflow`, `timing_late_events`,
`performance_dropped`, `automation_coalesced` and `reported_latency_samples`. These are per-instance/lifetime
diagnostics, not proof that hardware scheduling is accurate. Editor status says
whether timing is locked or unsynchronized. No new automation parameter, class
ID or saved-state format is introduced.

## Provenance

[TI Revive issue #5](https://github.com/Give-me-a-signal/virus-ti-rework/issues/5)
and evidence E139–E145 describe marker alignment, delayed SysEx and pacing.
Source comparison is pinned at `21b83e6f9c05497d8331023e9ff6845361c2d36c`.
The automation queue is adapted from that MIT project; its complete notice is
retained in `THIRD_PARTY_NOTICES.md` and packaged with both formats. Marker
detection and scheduling are adapted to this project's six-channel path;
TI Revive's broker, automatic bank sync, installer, Access resource model and
44.1 kHz constants are not imported.

## Checks performed and still required

Offline coverage includes false/cross-callback markers, cancellation/rearm,
14-bit wrap and horizon limits, fractional 44.1/48/96 kHz conversion, bounded
automation, committed-block ordering, reserved note-off capacity and panic,
confirmed-marker masking, and negative private-protocol capture audits.
The actual AU/VST3 bundles are loaded and rendered by `protocol_bench`, without
linking the processor into the host. No-Connect tests assert zero hardware
capture/MIDI activity. A second test holds the real lease and exercises three
refused connections in each format, with both processes alive; cleanup retains
the owner's lease. This is a refusal test with no device I/O, not a live-audio
ownership test.

**Full hardware acceptance is pending.** The initial 48 kHz/256-frame measurements
passed for VST3 on return 1 and AU on return 2, including sounding cross-format
refusal in the AU case. See the [dated validation record](ownership-timing-validation-2026-10-15.md)
for exact builds, measurements and outstanding checks. A marker-shaped response alone does not prove
the 48 kHz clock relationship. It must be checked using recorded stamped notes,
reported latency and all three returns before changing the default. A failed
experimental calibration is reported as unsynchronized; stop that bench and
verify restoration rather than repeatedly probing or assuming playback works.

`tools/test_recall_ownership_hardware.py` prepares bounded ten-cycle RAM recall,
cross-format refusal, manual restoration and normal-exit checks. It requires
explicit bundle paths and a fresh private output directory; it uses the synth's
captured sounds and reversible supported cutoff edits, not a factory fixture.
`protocol_bench` also accepts a fresh trigger-file path for 256 synthetic notes
at 120 BPM; it records all six output channels and prints the exact first-note
host frame. No trigger means no test notes. It never connects by itself.

Required follow-up is the 44.1/48/96 kHz and 64/256/512-frame timing matrix,
missing/extra-note analysis, corrected mean error <=1 ms, jitter SD <=0.5 ms,
drift <=10 ppm with a suitable transient patch, real-device ownership continuity,
all-part sound/routing readbacks, original 18-buffer restoration and Logic/
Bitwig save/reopen. Ableton saving and LiveProfessor remain deferred. TI2/Snow
are unverified. Raw recordings, patch/state data and recovery directories stay
private. Do not publish a timing or hardware pass from the offline results.
