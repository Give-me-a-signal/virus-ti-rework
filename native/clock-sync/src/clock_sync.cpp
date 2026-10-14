#include "virus/clock_sync.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace virus::clock {
namespace {
constexpr auto u64max = std::numeric_limits<std::uint64_t>::max();
double difference(std::uint64_t a, std::uint64_t b) noexcept {
    return a >= b ? static_cast<double>(a-b) : -static_cast<double>(b-a);
}
bool add_rounded(std::uint64_t base, double offset, std::uint64_t& output) noexcept {
    // Model deltas are bounded by the fixed window and configured gaps. Bound before floating->integer
    // conversion so even a hostile absolute sample/host value is safe.
    if (!std::isfinite(offset) || std::abs(offset) > 1e15) return false;
    auto magnitude = static_cast<std::uint64_t>(std::floor(std::abs(offset) + 0.5));
    if (offset < 0) { if (base < magnitude) return false; output = base - magnitude; }
    else { if (magnitude > u64max - base) return false; output = base + magnitude; }
    return true;
}
bool outside(std::uint64_t value, std::uint64_t first, std::uint64_t last,
             std::uint64_t margin) noexcept {
    return (value < first && first-value > margin) || (value > last && value-last > margin);
}
}
ClockEstimator::ClockEstimator(Config config) noexcept : config_(config) {
    valid_ = std::isfinite(config.nominal_rate) && config.nominal_rate >= 8000 &&
        config.nominal_rate <= 384000 && std::isfinite(config.maximum_rate_error_ppm) &&
        config.maximum_rate_error_ppm > 0 && config.maximum_rate_error_ppm <= 10000 &&
        config.minimum_points >= 3 && config.minimum_points <= capacity &&
        config.minimum_fit_span_ns > 0 && config.minimum_fit_span_ns <= 60000000000ULL &&
        config.maximum_gap_ns >= config.minimum_fit_span_ns &&
        config.maximum_gap_ns <= 60000000000ULL && config.maximum_extrapolation_ns <= 1000000000 &&
        config.outlier_limit_ns > 0 && config.outlier_limit_ns <= config.maximum_gap_ns &&
        config.uncertainty_floor_ns <= config.outlier_limit_ns;
    model_.kind = config.kind;
    model_.rate = config.nominal_rate;
    model_.epoch = 1;
}
void ClockEstimator::reset() noexcept {
    count_ = 0;
    fit_ = {};
    model_.ready = false;
    model_.points = 0;
    model_.rate = config_.nominal_rate;
    model_.rate_error_ppm = model_.residual_rms_ns = model_.uncertainty_ns = 0;
    if (model_.epoch != u64max) ++model_.epoch;
    ++model_.resets;
}
void ClockEstimator::anchor(Point point) noexcept {
    points_[0] = point;
    count_ = model_.points = 1;
    ++model_.accepted;
    fit_ = {config_.nominal_rate / 1e9, 0, 0, 0};
}
bool ClockEstimator::fit(const std::array<Point, capacity>& points, std::size_t count, Fit& out) const noexcept {
    double sx = 0, sy = 0;
    for (std::size_t i = 0; i < count; ++i) {
        sx += difference(points[i].host.value, points[0].host.value);
        sy += difference(points[i].frame.value, points[0].frame.value);
    }
    const double mx = sx / count, my = sy / count;
    double xx = 0, xy = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto x = difference(points[i].host.value, points[0].host.value) - mx;
        const auto y = difference(points[i].frame.value, points[0].frame.value) - my;
        xx += x*x; xy += x*y;
    }
    if (!(xx > 0)) return false;
    out.frames_per_ns = xy/xx;
    out.intercept_frames = my-out.frames_per_ns*mx;
    if (!(out.frames_per_ns > 0) || !std::isfinite(out.frames_per_ns)) return false;
    double squared = 0;
    out.maximum_ns = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto residual = (difference(points[i].frame.value, points[0].frame.value) -
            out.intercept_frames)/out.frames_per_ns - difference(points[i].host.value, points[0].host.value);
        squared += residual*residual;
        out.maximum_ns = std::max(out.maximum_ns, std::abs(residual));
    }
    out.rms_ns = std::sqrt(squared/count);
    return std::isfinite(out.rms_ns);
}
ObserveStatus ClockEstimator::observe(SampleFrame frame, HostTimeNs host) noexcept {
    if (!valid_) return ObserveStatus::invalid_configuration;
    Point point{frame,host};
    if (!count_) { anchor(point); return ObserveStatus::anchored; }
    const auto last = points_[count_-1];
    if (host.value < last.host.value || frame.value < last.frame.value ||
        host.value-last.host.value > config_.maximum_gap_ns) {
        reset(); anchor(point); return ObserveStatus::reset_discontinuity;
    }
    if (host.value == last.host.value) return ObserveStatus::duplicate_host_time;
    if (frame.value == last.frame.value) return ObserveStatus::invalid_observation;
    if (host.value-last.host.value < std::max<std::uint64_t>(1,config_.minimum_fit_span_ns/32))
        return ObserveStatus::insufficient_spacing;
    // Bound the mathematical problem before fitting. A huge unsigned counter
    // jump must not become a plausible low precision floating-point anchor.
    const double maximum_frames = config_.nominal_rate * config_.maximum_gap_ns / 1e9 * 1.02;
    if (static_cast<double>(frame.value-last.frame.value) > maximum_frames) {
        ++model_.rejected; return ObserveStatus::rejected_outlier;
    }
    const auto predicted_ns = (difference(frame.value,points_[0].frame.value)-fit_.intercept_frames)/fit_.frames_per_ns;
    const auto error_ns = predicted_ns - difference(host.value,points_[0].host.value);
    // Before a rate fit, allow the configured oscillator skew across the span.
    const auto skew_allowance = model_.ready ? 0.0 :
        difference(host.value, points_[0].host.value)*config_.maximum_rate_error_ppm/1e6;
    if (std::abs(error_ns) > config_.outlier_limit_ns + skew_allowance) {
        ++model_.rejected; return ObserveStatus::rejected_outlier;
    }
    auto candidate = points_;
    auto candidate_count = count_;
    if (candidate_count == capacity) {
        for (std::size_t i = 1; i < capacity; ++i) candidate[i-1] = candidate[i];
        --candidate_count;
    }
    candidate[candidate_count++] = point;
    const auto span = host.value-candidate[0].host.value;
    const bool enough = candidate_count >= config_.minimum_points && span >= config_.minimum_fit_span_ns;
    Fit next = {config_.nominal_rate/1e9,0,0,0};
    if (enough) {
        if (!fit(candidate,candidate_count,next) ||
            std::abs(next.frames_per_ns*1e9/config_.nominal_rate-1.0)*1e6 > config_.maximum_rate_error_ppm ||
            next.maximum_ns > config_.outlier_limit_ns) {
            ++model_.rejected; return ObserveStatus::rejected_outlier;
        }
    }
    points_ = candidate; count_ = candidate_count; fit_ = next;
    ++model_.accepted;
    model_.points = count_;
    model_.ready = enough;
    model_.rate = fit_.frames_per_ns*1e9;
    model_.rate_error_ppm = (model_.rate/config_.nominal_rate-1.0)*1e6;
    model_.residual_rms_ns = next.rms_ns;
    model_.uncertainty_ns = std::max(static_cast<double>(config_.uncertainty_floor_ns),next.maximum_ns);
    return ObserveStatus::accepted;
}
HostEstimate ClockEstimator::host_at(SampleFrame frame) const noexcept {
    if (!model_.ready) return {MapStatus::not_ready,{},0};
    const auto delta = (difference(frame.value,points_[0].frame.value)-fit_.intercept_frames)/fit_.frames_per_ns;
    std::uint64_t value;
    if (!add_rounded(points_[0].host.value,delta,value)) return {MapStatus::overflow,{},0};
    if (outside(value,points_[0].host.value,points_[count_-1].host.value,config_.maximum_extrapolation_ns))
        return {MapStatus::outside_horizon,{},0};
    return {MapStatus::ok,{value},model_.uncertainty_ns};
}
FrameEstimate ClockEstimator::frame_at(HostTimeNs host) const noexcept {
    if (!model_.ready) return {MapStatus::not_ready,{},0};
    if (outside(host.value,points_[0].host.value,points_[count_-1].host.value,config_.maximum_extrapolation_ns))
        return {MapStatus::outside_horizon,{},0};
    const auto delta = difference(host.value,points_[0].host.value)*fit_.frames_per_ns+fit_.intercept_frames;
    std::uint64_t value;
    if (!add_rounded(points_[0].frame.value,delta,value)) return {MapStatus::overflow,{},0};
    return {MapStatus::ok,{value},model_.uncertainty_ns*fit_.frames_per_ns+0.5};
}
FrameEstimate bridge_frame(const ClockEstimator& source, SampleFrame frame, const ClockEstimator& target) noexcept {
    const auto host = source.host_at(frame);
    if (host.status != MapStatus::ok) return {host.status,{},0};
    auto result = target.frame_at(host.time);
    if (result.status == MapStatus::ok) result.uncertainty_frames += host.uncertainty_ns*target.model().rate/1e9;
    return result;
}
Raw14Unwrapper::Raw14Unwrapper(std::uint16_t delta, std::uint64_t gap) noexcept
    : maximum_delta_(delta), maximum_gap_(gap) {
    poisoned_ = !delta || delta >= 8192 || !gap;
}
void Raw14Unwrapper::reset() noexcept {
    anchored_ = false;
    poisoned_ = !maximum_delta_ || maximum_delta_ >= 8192 || !maximum_gap_;
    if (epoch_ != u64max) ++epoch_;
}
UnwrapResult Raw14Unwrapper::observe(std::uint16_t raw, HostTimeNs host) noexcept {
    auto result = [&](UnwrapStatus status) { return UnwrapResult{status,{extended_},epoch_}; };
    if (raw >= 16384) return result(UnwrapStatus::invalid_raw);
    if (poisoned_) return result(UnwrapStatus::needs_reset);
    if (!anchored_) {
        raw_ = raw; extended_ = raw; last_host_ = host.value; anchored_ = true;
        return result(UnwrapStatus::anchored);
    }
    const auto delta = static_cast<std::uint16_t>((raw-raw_)&0x3fff);
    if (host.value < last_host_ || host.value-last_host_ > maximum_gap_ || delta > maximum_delta_) {
        poisoned_ = true; return result(UnwrapStatus::ambiguous);
    }
    if (delta > u64max-extended_) { poisoned_ = true; return result(UnwrapStatus::overflow); }
    extended_ += delta; raw_ = raw; last_host_ = host.value;
    return result(delta ? UnwrapStatus::advanced : UnwrapStatus::repeated);
}
PrivateDeadline private_deadline(const TickCalibration& calibration, SampleFrame frame,
                                 PrivateTick now, std::uint16_t horizon) noexcept {
    if (!calibration.verified || !calibration.ticks_numerator || !calibration.samples_denominator)
        return {DeadlineStatus::uncalibrated,{},0};
    if (!horizon || horizon >= 8192) return {DeadlineStatus::outside_horizon,{},0};
    if (frame.value < calibration.anchor_frame.value) return {DeadlineStatus::past,{},0};
    // Widen before multiplication, round toward the future; 64*32 <= 96 bits.
    const auto scaled = static_cast<__uint128_t>(frame.value-calibration.anchor_frame.value)*calibration.ticks_numerator;
    const auto delta = (scaled+calibration.samples_denominator-1)/calibration.samples_denominator;
    if (delta > u64max-calibration.anchor_tick.value) return {DeadlineStatus::overflow,{},0};
    const auto tick = calibration.anchor_tick.value+static_cast<std::uint64_t>(delta);
    if (tick < now.value) return {DeadlineStatus::past,{},0};
    if (tick-now.value > horizon) return {DeadlineStatus::outside_horizon,{},0};
    return {DeadlineStatus::ok,{tick},static_cast<std::uint16_t>(tick&0x3fff)};
}
} // namespace virus::clock
