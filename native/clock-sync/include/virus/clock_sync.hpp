#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace virus::clock {
// All callers must use the SAME monotonic host clock and normalize its units.
// A host timestamp is not a USB service ordinal or a private Virus tick.
struct HostTimeNs { std::uint64_t value = 0; };
struct SampleFrame { std::uint64_t value = 0; };
struct PrivateTick { std::uint64_t value = 0; };
enum class ObservationKind { sample_anchor, usb_completion };
struct Config {
    double nominal_rate = 48000.0;
    double maximum_rate_error_ppm = 2000.0;
    std::uint64_t minimum_fit_span_ns = 250000000;
    std::uint64_t maximum_gap_ns = 2000000000;
    std::uint64_t maximum_extrapolation_ns = 100000000;
    std::uint64_t outlier_limit_ns = 3000000;
    std::uint64_t uncertainty_floor_ns = 125000;
    std::size_t minimum_points = 8;
    ObservationKind kind = ObservationKind::usb_completion;
};
enum class ObserveStatus {
    anchored, accepted, duplicate_host_time, insufficient_spacing, rejected_outlier,
    reset_discontinuity, invalid_configuration, invalid_observation
};
enum class MapStatus { ok, not_ready, outside_horizon, overflow };
struct Model {
    bool ready = false;
    ObservationKind kind = ObservationKind::usb_completion;
    std::uint64_t epoch = 0, accepted = 0, rejected = 0, resets = 0;
    std::size_t points = 0;
    double rate = 0, rate_error_ppm = 0;
    // Empirical fit residual, NOT a statistical confidence interval and NOT
    // device/DAW latency. A constant USB interrupt delay cannot be identified.
    double residual_rms_ns = 0, uncertainty_ns = 0;
};
struct HostEstimate { MapStatus status; HostTimeNs time; double uncertainty_ns; };
struct FrameEstimate { MapStatus status; SampleFrame frame; double uncertainty_frames; };

// NEW diagnostic model, not a reconstruction of Access's PLL. Fixed memory,
// bounded O(64) work, no allocation/locks. Samples closer than minimum_fit_span /
// 32 are decimated, so a 1 kHz completion stream cannot evict all useful history.
// Single executor; not thread safe.
// Do not drive a resampler or transmit private tick messages from this alone.
class ClockEstimator {
public:
    static constexpr std::size_t capacity = 64;
    explicit ClockEstimator(Config = {}) noexcept;
    bool valid() const noexcept { return valid_; }
    void reset() noexcept;
    ObserveStatus observe(SampleFrame, HostTimeNs) noexcept;
    HostEstimate host_at(SampleFrame) const noexcept;
    FrameEstimate frame_at(HostTimeNs) const noexcept;
    const Model& model() const noexcept { return model_; }
private:
    struct Point { SampleFrame frame; HostTimeNs host; };
    struct Fit { double frames_per_ns = 0, intercept_frames = 0, rms_ns = 0, maximum_ns = 0; };
    bool fit(const std::array<Point, capacity>&, std::size_t, Fit&) const noexcept;
    void anchor(Point) noexcept;
    Config config_;
    bool valid_ = false;
    std::array<Point, capacity> points_{};
    std::size_t count_ = 0;
    Fit fit_{};
    Model model_{};
};

// Bridge TWO independently calibrated sample domains through host time. This
// does not remove clock drift; the caller must still choose a resampling policy.
FrameEstimate bridge_frame(const ClockEstimator& source, SampleFrame source_frame,
                           const ClockEstimator& target) noexcept;

enum class UnwrapStatus {
    anchored, advanced, repeated, invalid_raw, ambiguous, needs_reset, overflow
};
struct UnwrapResult { UnwrapStatus status; PrivateTick tick; std::uint64_t epoch; };
class Raw14Unwrapper {
public:
    // maximum_forward_delta must be 1..8191. maximum_gap_ns is an independently
    // justified observation-gap limit; no Virus tick frequency is assumed.
    explicit Raw14Unwrapper(std::uint16_t maximum_forward_delta = 4096,
                            std::uint64_t maximum_gap_ns = 100000000) noexcept;
    void reset() noexcept;
    UnwrapResult observe(std::uint16_t raw, HostTimeNs observed_at) noexcept;
    bool requires_reset() const noexcept { return poisoned_; }
private:
    std::uint16_t maximum_delta_, raw_ = 0;
    std::uint64_t maximum_gap_, last_host_ = 0, epoch_ = 1, extended_ = 0;
    bool anchored_ = false, poisoned_ = false;
};

// Only a caller with a measured/proven relationship can enable this calibration.
// The known 53 F5 lo7 hi7 word alone establishes NO tick/sample conversion.
struct TickCalibration {
    bool verified = false;
    SampleFrame anchor_frame;
    PrivateTick anchor_tick;
    std::uint32_t ticks_numerator = 0, samples_denominator = 0;
};
enum class DeadlineStatus { ok, uncalibrated, past, outside_horizon, overflow };
struct PrivateDeadline { DeadlineStatus status; PrivateTick tick; std::uint16_t raw14; };
PrivateDeadline private_deadline(const TickCalibration&, SampleFrame desired_frame,
                                 PrivateTick now, std::uint16_t horizon = 4096) noexcept;
} // namespace virus::clock
