#include "virus/clock_sync.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace virus::clock;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::abort(); } } while(false)
constexpr auto max64 = std::numeric_limits<std::uint64_t>::max();
void basic_and_boundaries() {
    ClockEstimator clock;
    CHECK(clock.valid());
    CHECK(clock.host_at({0}).status == MapStatus::not_ready);
    const std::uint64_t host_origin = 9000000000000000000ULL;
    const std::uint64_t frame_origin = 17000000000000000000ULL;
    for (std::uint64_t i = 0; i < 1000; ++i) {
        auto status = clock.observe({frame_origin+i*480},{host_origin+i*10000000});
        CHECK(status == (i ? ObserveStatus::accepted : ObserveStatus::anchored));
    }
    CHECK(clock.model().ready && clock.model().points == 64);
    CHECK(std::abs(clock.model().rate-48000) < 1e-6);
    auto host = clock.host_at({frame_origin+999*480});
    CHECK(host.status == MapStatus::ok && host.time.value == host_origin+9990000000);
    auto frame = clock.frame_at({host_origin+9990000000});
    CHECK(frame.status == MapStatus::ok && frame.frame.value == frame_origin+999*480);
    CHECK(clock.frame_at({host_origin}).status == MapStatus::outside_horizon);
    CHECK(clock.frame_at({host_origin+10100000000}).status == MapStatus::outside_horizon);
    CHECK(clock.host_at({max64}).status != MapStatus::ok);
    CHECK(clock.host_at({0}).status != MapStatus::ok);
    CHECK(clock.observe({frame_origin+1000*480},{host_origin+9990000000}) == ObserveStatus::duplicate_host_time);
    CHECK(clock.model().points == 64);
    CHECK(clock.observe({frame_origin+999*480},{host_origin+10000000000}) == ObserveStatus::invalid_observation);
    auto rejected = clock.observe({frame_origin+1001*480},{host_origin+10020000000});
    CHECK(rejected == ObserveStatus::rejected_outlier);
    CHECK(clock.model().rejected == 1);
    CHECK(clock.observe({frame_origin+1002*480},{host_origin+10020000000}) == ObserveStatus::accepted);
    CHECK(clock.observe({100},{host_origin+10030000000}) == ObserveStatus::reset_discontinuity);
    CHECK(!clock.model().ready && clock.model().epoch == 2);
    CHECK(clock.observe({200},{host_origin+13030000000}) == ObserveStatus::reset_discontinuity);
    CHECK(clock.model().epoch == 3);
    CHECK(clock.observe({201},{host_origin}) == ObserveStatus::reset_discontinuity);
    clock.reset(); CHECK(clock.model().epoch == 5 && !clock.model().ready);
    Config bad;
    bad.nominal_rate = std::numeric_limits<double>::quiet_NaN();
    ClockEstimator invalid(bad);
    CHECK(!invalid.valid());
    CHECK(invalid.observe({0},{0}) == ObserveStatus::invalid_configuration);
    bad = {}; bad.minimum_points = 65;
    CHECK(!ClockEstimator(bad).valid());
    bad = {}; bad.maximum_rate_error_ppm = std::numeric_limits<double>::infinity();
    CHECK(!ClockEstimator(bad).valid());
}
void synthetic_skew_jitter() {
    for (double nominal : {44100.0,48000.0,96000.0}) {
        for (double ppm : {-1200.0,-250.0,0.0,250.0,1200.0}) {
            Config config;
            config.nominal_rate = nominal;
            ClockEstimator clock(config);
            const auto actual_rate = nominal*(1+ppm/1e6);
            std::uint32_t random = 19223;
            const std::uint64_t origin = 7000000000000000000ULL;
            double maximum_rate_error = 0;
            for (std::uint64_t i = 0; i <= 12000; ++i) {
                // 120 seconds, true device samples, monotonic completion clock.
                const auto frame = static_cast<std::uint64_t>(std::llround(i*.01*actual_rate));
                random = random*1664525u+1013904223u;
                const auto jitter = static_cast<std::int64_t>(random%60001)-30000;
                const auto host = origin+i*10000000+100000+static_cast<std::uint64_t>(jitter+30000);
                const auto status = clock.observe({frame},{host});
                CHECK(status == ObserveStatus::anchored || status == ObserveStatus::accepted);
                if (i > 100) {
                    CHECK(clock.model().ready);
                    maximum_rate_error = std::max(maximum_rate_error,std::abs(clock.model().rate/actual_rate-1)*1e6);
                    const auto estimate = clock.host_at({frame});
                    CHECK(estimate.status == MapStatus::ok);
                    const auto expected = origin+i*10000000+130000;
                    const auto error = estimate.time.value > expected ? estimate.time.value-expected : expected-estimate.time.value;
                    CHECK(error < 30000);
                }
            }
            CHECK(maximum_rate_error < 110);
            CHECK(std::abs(clock.model().rate_error_ppm-ppm) < 110);
            CHECK(clock.model().residual_rms_ns < 35000);
        }
    }
    // Dense 1 kHz inputs must retain enough span, including duplicate batched
    // completion timestamps that cannot identify individual packet sample time.
    ClockEstimator dense;
    std::size_t decimated = 0;
    for (std::uint64_t i = 0; i < 10000; ++i) {
        auto status = dense.observe({i*48},{i*1000000});
        if (status == ObserveStatus::insufficient_spacing) ++decimated;
    }
    CHECK(decimated > 8000 && dense.model().ready);
    CHECK(std::abs(dense.model().rate-48000) < 1e-6);
}
void bridge_and_overflow() {
    Config config;
    config.kind = ObservationKind::sample_anchor;
    ClockEstimator source(config);
    config.nominal_rate = 44100;
    ClockEstimator target(config);
    for (std::uint64_t i = 0; i < 64; ++i) {
        source.observe({1000+i*480},{9000000000+i*10000000});
        target.observe({8000+i*441},{9000000000+i*10000000});
    }
    const auto bridge = bridge_frame(source,{1000+50*480},target);
    CHECK(bridge.status == MapStatus::ok && bridge.frame.value == 8000+50*441);
    CHECK(bridge.uncertainty_frames >= 11);
    ClockEstimator not_ready;
    CHECK(bridge_frame(source,{1000+50*480},not_ready).status == MapStatus::not_ready);
    ClockEstimator upper;
    for (std::uint64_t i = 0; i < 64; ++i)
        upper.observe({i*480},{max64-630000000+i*10000000});
    CHECK(upper.model().ready);
    CHECK(upper.host_at({64*480}).status == MapStatus::overflow);
    ClockEstimator lower;
    for (std::uint64_t i = 0; i < 64; ++i) lower.observe({1000+i*480},{i*10000000});
    CHECK(lower.host_at({999}).status == MapStatus::overflow);
    ClockEstimator frames_upper;
    for (std::uint64_t i = 0; i < 64; ++i)
        frames_upper.observe({max64-63*480+i*480},{i*10000000});
    CHECK(frames_upper.frame_at({640000000}).status == MapStatus::overflow);
}
void unwrap_and_deadline() {
    Raw14Unwrapper unwrap(8191,10000);
    CHECK(unwrap.observe(16380,{100}).status == UnwrapStatus::anchored);
    auto wrapped = unwrap.observe(2,{200});
    CHECK(wrapped.status == UnwrapStatus::advanced && wrapped.tick.value == 16386);
    CHECK(unwrap.observe(2,{200}).status == UnwrapStatus::repeated);
    CHECK(unwrap.observe(16384,{300}).status == UnwrapStatus::invalid_raw);
    CHECK(!unwrap.requires_reset());
    CHECK(unwrap.observe(8194,{300}).status == UnwrapStatus::ambiguous);
    CHECK(unwrap.requires_reset());
    CHECK(unwrap.observe(3,{301}).status == UnwrapStatus::needs_reset);
    unwrap.reset();
    CHECK(unwrap.observe(8,{100}).status == UnwrapStatus::anchored);
    CHECK(unwrap.observe(9,{50}).status == UnwrapStatus::ambiguous);
    unwrap.reset(); unwrap.observe(8,{100});
    CHECK(unwrap.observe(9,{10101}).status == UnwrapStatus::ambiguous);
    Raw14Unwrapper invalid(8192,100);
    CHECK(invalid.observe(1,{1}).status == UnwrapStatus::needs_reset);
    invalid.reset(); CHECK(invalid.observe(1,{1}).status == UnwrapStatus::needs_reset);
    // Exhaustive starting raw values with forward wrap and backward ambiguity.
    for (std::uint32_t raw = 0; raw < 16384; ++raw) {
        Raw14Unwrapper sequence(8191,1000);
        sequence.observe(static_cast<std::uint16_t>(raw),{100});
        const auto next = static_cast<std::uint16_t>((raw+333)&0x3fff);
        CHECK(sequence.observe(next,{200}).tick.value == raw+333);
        CHECK(sequence.observe(static_cast<std::uint16_t>((next-1)&0x3fff),{300}).status == UnwrapStatus::ambiguous);
    }
    Raw14Unwrapper many_wraps;
    many_wraps.observe(0,{0});
    for (std::uint64_t tick = 1; tick <= 1000000; ++tick) {
        const auto result = many_wraps.observe(static_cast<std::uint16_t>(tick&0x3fff),{tick});
        CHECK(result.status == UnwrapStatus::advanced && result.tick.value == tick);
    }
    TickCalibration calibration;
    CHECK(private_deadline(calibration,{100},{0}).status == DeadlineStatus::uncalibrated);
    calibration = {true,{100},{16380},1,2};
    const auto deadline = private_deadline(calibration,{111},{16380});
    CHECK(deadline.status == DeadlineStatus::ok && deadline.tick.value == 16386 && deadline.raw14 == 2);
    CHECK(private_deadline(calibration,{99},{0}).status == DeadlineStatus::past);
    CHECK(private_deadline(calibration,{100},{16381}).status == DeadlineStatus::past);
    CHECK(private_deadline(calibration,{8300},{16380}).status == DeadlineStatus::outside_horizon);
    CHECK(private_deadline(calibration,{100},{16380},8192).status == DeadlineStatus::outside_horizon);
    calibration = {true,{0},{max64},1,1};
    CHECK(private_deadline(calibration,{1},{max64}).status == DeadlineStatus::overflow);
    calibration = {true,{0},{0},0xffffffff,1};
    CHECK(private_deadline(calibration,{max64},{0}).status == DeadlineStatus::overflow);
    calibration.samples_denominator = 0;
    CHECK(private_deadline(calibration,{0},{0}).status == DeadlineStatus::uncalibrated);
}
int main() {
    basic_and_boundaries(); synthetic_skew_jitter(); bridge_and_overflow(); unwrap_and_deadline();
    std::puts("clock-sync: 15 x 120-second skew/jitter cases, dense completion decimation, domain bridge, overflow/discontinuity/outlier checks, 16384 wrap vectors and 1000000 sequential ticks passed");
}
