#include "virus/transport_core.hpp"

#include <iostream>
#include <thread>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const char* label) {
    if (!condition) throw std::runtime_error(label);
}

template <class Function>
void rejects(Function function, const char* label) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(std::string("accepted invalid input: ") + label);
}
void bounded_pcm_tests() {
    using virus::BufferStatus;
    using virus::PcmFormat;
    const auto f16 = PcmFormat::signed_16_le;
    const auto f24 = PcmFormat::signed_24_le;
    const std::vector<std::uint8_t> packed16{0, 0, 1, 0, 0xff, 0x7f, 0, 0x80,
                                           0xff, 0xff, 0x34, 0x12};
    const std::vector<std::int32_t> values16{0, 65536, 2147418112, -2147483647 - 1,
                                            -65536, 305397760};
    check(virus::pcm_from_16_lsb(packed16, 2) == values16, "16-bit explicit decode");
    check(virus::pcm_to_16_lsb(values16, 2) == packed16, "16-bit explicit encode");
    check(virus::pcm_to_16_lsb({2147483647, -2147483647 - 1, -1, 65535, -65535, 65537}) ==
              std::vector<std::uint8_t>{0xff, 0x7f, 0, 0x80, 0xff, 0xff, 0, 0,
                                       0xff, 0xff, 1, 0}, "16-bit truncation");
    for (auto format : {f16, f24}) {
        const std::size_t n = static_cast<std::size_t>(format);
        // Deliberately unaligned packed source/destination with surrounding guards.
        std::array<std::uint8_t, 14> packed{};
        packed.fill(0xcc);
        const std::array<std::int32_t, 4> samples{0x12340000, -2147483647 - 1, -65536, 0};
        check(virus::pcm_encode(format, samples.data(), 4, 2, packed.data() + 1, 4*n) ==
                  BufferStatus::ok, "bounded unaligned encode");
        check(packed.front() == 0xcc && packed[1+4*n] == 0xcc, "packed guards");
        std::array<std::int32_t, 6> decoded{};
        decoded.fill(123);
        check(virus::pcm_decode(format, packed.data() + 1, 4*n, 2, decoded.data()+1, 4) ==
                  BufferStatus::ok, "bounded unaligned decode");
        check(std::equal(samples.begin(), samples.end(), decoded.begin()+1), "bounded values");
        check(decoded.front() == 123 && decoded.back() == 123, "sample guards");
        const auto saved = decoded;
        check(virus::pcm_decode(format, packed.data()+1, 4*n, 2, decoded.data(), 3) ==
                  BufferStatus::insufficient_capacity, "decode capacity");
        check(decoded == saved, "decode capacity rejection atomic");
        check(virus::pcm_decode(format, packed.data()+1, 3*n, 2, decoded.data(), 6) ==
                  BufferStatus::incomplete_frame, "decode partial frame");
        check(decoded == saved, "decode partial rejection atomic");
        const auto saved_bytes = packed;
        check(virus::pcm_encode(format, samples.data(), 4, 2, packed.data(), 4*n-1) ==
                  BufferStatus::insufficient_capacity, "encode capacity");
        check(virus::pcm_encode(format, samples.data(), 3, 2, packed.data(), 14) ==
                  BufferStatus::incomplete_frame, "encode partial frame");
        check(packed == saved_bytes, "encode rejection atomic");
        check(virus::pcm_decode(format, nullptr, 0, 2, nullptr, 0) == BufferStatus::ok,
              "bounded empty decode");
        check(virus::pcm_encode(format, nullptr, 0, 2, nullptr, 0) == BufferStatus::ok,
              "bounded empty encode");
        check(virus::pcm_decode(format, nullptr, 2*n, 2, decoded.data(), 6) ==
                  BufferStatus::invalid_argument, "decode null input");
        check(virus::pcm_encode(format, samples.data(), 4, 2, nullptr, 4*n) ==
                  BufferStatus::invalid_argument, "encode null output");
        check(virus::pcm_encode(format, samples.data(), std::numeric_limits<std::size_t>::max(),
                                1, packed.data(), 14) == BufferStatus::size_overflow,
              "sample count overflow rejected before read");
        check(virus::pcm_decode(format, nullptr, 0, std::numeric_limits<std::size_t>::max(),
                                nullptr, 0) == BufferStatus::size_overflow, "channel overflow");
        std::array<std::int32_t, 4> alias{123, 456, 789, 111};
        const auto alias_saved = alias;
        check(virus::pcm_decode(format, reinterpret_cast<const std::uint8_t*>(alias.data()),
                                4*n, 2, alias.data(), 4) == BufferStatus::overlapping_buffers,
              "decode overlap rejected");
        check(virus::pcm_encode(format, alias.data(), 4, 2,
                                reinterpret_cast<std::uint8_t*>(alias.data()), 16) ==
                  BufferStatus::overlapping_buffers, "encode overlap rejected");
        check(alias == alias_saved, "overlap rejection atomic");
    }
    check(virus::pcm_decode(static_cast<PcmFormat>(4), nullptr, 0, 1, nullptr, 0) ==
              BufferStatus::invalid_argument, "invalid PCM format");
}

void pacing_tests() {
    using virus::BufferStatus;
    using virus::PcmFormat;
    for (const std::uint32_t rate : {44100u, 48000u}) {
        for (const std::uint32_t intervals : {1000u, 8000u}) {
            virus::AudioPacketConfig config{rate, intervals, 6, PcmFormat::signed_24_le, 864};
            virus::AudioPacketBounds bounds{};
            check(virus::validate_audio_packet_config(config, bounds) == BufferStatus::ok,
                  "valid packet config");
            check(bounds.bytes_per_frame == 18, "packet frame bytes");
            const std::size_t expected_max = intervals == 8000 ? 6 : rate == 44100 ? 45 : 48;
            check(bounds.maximum_frames == expected_max &&
                  bounds.maximum_bytes == expected_max * 18, "independent packet maximum");
            virus::NominalPacketSchedule schedule(config);
            std::uint64_t total = 0;
            for (std::uint64_t i = 1; i <= std::uint64_t{intervals} * 60; ++i) {
                const auto frames = schedule.next_frames();
                check(frames <= expected_max, "packet bound each interval");
                total += frames;
                check(total == i * rate / intervals, "exact prefix schedule, no drift");
            }
            check(total == std::uint64_t{rate} * 60, "one minute exact sample total");
            schedule.reset();
            check(schedule.next_frames() == rate / intervals, "schedule reset phase");
            config.max_packet_bytes = bounds.maximum_bytes - 1;
            const auto saved = bounds;
            check(virus::validate_audio_packet_config(config, bounds) ==
                      BufferStatus::insufficient_capacity, "packet capacity rejection");
            check(bounds.maximum_bytes == saved.maximum_bytes, "bounds rejection unchanged");
            rejects([&] { virus::NominalPacketSchedule invalid(config); }, "schedule capacity");
        }
    }
    virus::AudioPacketBounds bounds{};
    check(virus::validate_audio_packet_config({44100, 1000, std::numeric_limits<std::size_t>::max(),
                PcmFormat::signed_24_le, 1024}, bounds) == BufferStatus::size_overflow,
          "packet size overflow");
    check(virus::validate_audio_packet_config({44100, 0, 2, PcmFormat::signed_16_le, 1024},
                bounds) == BufferStatus::invalid_argument, "invalid interval");
    check(virus::validate_audio_packet_config({96000, 1000, 2, PcmFormat::signed_16_le, 1024},
                bounds) == BufferStatus::invalid_argument, "invalid rate");
}

void queue_tests() {
    using virus::QueueStatus;
    rejects([] { virus::AudioFrameQueue invalid(0, 2); }, "zero queue");
    rejects([] { virus::AudioFrameQueue invalid(3, 2); }, "non power of two queue");
    rejects([] { virus::AudioFrameQueue invalid(4, 0); }, "zero queue channels");
    rejects([] { virus::AudioFrameQueue invalid(4, std::numeric_limits<std::size_t>::max()); },
            "queue allocation overflow");
    virus::AudioFrameQueue queue(4, 2);
    const std::array<std::int32_t, 8> input{1, -1, 2, -2, 3, -3, 4, -4};
    std::array<std::int32_t, 8> output{};
    output.fill(99);
    check(queue.pop(output.data(), 1, 8) == QueueStatus::underrun && output[0] == 99,
          "underrun leaves output untouched");
    check(queue.push(input.data(), 3, 5) == QueueStatus::insufficient_capacity,
          "push capacity rejection");
    check(queue.push(input.data(), 3, 8) == QueueStatus::ok, "initial push");
    check(queue.push(input.data(), 2, 8) == QueueStatus::overrun, "overrun explicit");
    check(queue.pop(output.data(), 2, 3) == QueueStatus::insufficient_capacity,
          "pop capacity rejection");
    check(queue.pop(output.data(), 2, 8) == QueueStatus::ok && output[0] == 1 &&
          output[1] == -1 && output[2] == 2 && output[3] == -2, "queue first frames");
    check(queue.push(input.data(), 3, 8) == QueueStatus::ok, "wrap push");
    check(queue.pop(output.data(), 4, 8) == QueueStatus::ok &&
          output == std::array<std::int32_t, 8>{3, -3, 1, -1, 2, -2, 3, -3},
          "wrap preserves frames and channels");
    check(queue.pop(output.data(), 1, 8) == QueueStatus::underrun, "exact empty");
    check(queue.push(nullptr, 0, 0) == QueueStatus::ok &&
          queue.pop(nullptr, 0, 0) == QueueStatus::ok, "empty queue operations");
    check(queue.push(nullptr, 1, 2) == QueueStatus::invalid_argument, "queue null input");
    check(queue.pop(nullptr, 1, 2) == QueueStatus::invalid_argument, "queue null output");

    // Independent producer/consumer transfer ordered stereo frames through many wraps.
    virus::AudioFrameQueue concurrent(128, 2);
    std::atomic<bool> valid{true};
    constexpr std::int32_t count = 200000;
    std::thread producer([&] {
        for (std::int32_t frame = 0; frame < count; ++frame) {
            const std::int32_t stereo[2]{frame, -frame};
            QueueStatus status;
            do { status = concurrent.push(stereo, 1, 2); }
            while (status == QueueStatus::overrun);
            if (status != QueueStatus::ok) valid.store(false);
        }
    });
    std::thread consumer([&] {
        for (std::int32_t frame = 0; frame < count; ++frame) {
            std::int32_t stereo[2]{};
            QueueStatus status;
            do { status = concurrent.pop(stereo, 1, 2); }
            while (status == QueueStatus::underrun);
            if (status != QueueStatus::ok || stereo[0] != frame || stereo[1] != -frame)
                valid.store(false);
        }
    });
    producer.join();
    consumer.join();
    check(valid.load(), "threaded SPSC ordered stereo stress");
}
}  // namespace

int main() {
    try {
        bounded_pcm_tests();
        pacing_tests();
        queue_tests();
        const auto a = virus::make_sample_frequency_request(0x01, 44100);
        check(a.setup == std::array<std::uint8_t, 8>{0x22, 1, 0, 1, 1, 0, 3, 0},
              "44100 setup packet");
        check(a.payload == std::array<std::uint8_t, 3>{0x44, 0xac, 0}, "44100 payload");
        const auto b = virus::make_sample_frequency_request(0x8f, 48000);
        check(b.setup == std::array<std::uint8_t, 8>{0x22, 1, 0, 1, 0x8f, 0, 3, 0},
              "48000 setup packet preserves endpoint direction");
        check(b.payload == std::array<std::uint8_t, 3>{0x80, 0xbb, 0}, "48000 payload");
        for (const std::uint32_t endpoint : {0u, 0x80u, 0x10u, 0x91u, 0x41u, 0x101u,
                                            0xffffffffu}) {
            rejects([=] { virus::make_sample_frequency_request(endpoint, 48000); },
                    "endpoint address");
        }
        for (const std::uint32_t rate : {0u, 44099u, 44101u, 96000u, 0xffffffffu}) {
            rejects([=] { virus::make_sample_frequency_request(1, rate); }, "sample rate");
        }
        // Expected numeric values are explicit, independent of the inverse code.
        const std::vector<std::uint8_t> bytes{0, 0, 0, 1, 0, 0, 0xff, 0xff, 0x7f,
                                            0, 0, 0x80, 0xff, 0xff, 0xff, 0x56, 0x34, 0x12};
        const std::vector<std::int32_t> values{0, 256, 2147483392, -2147483647 - 1,
                                              -256, 305419776};
        check(virus::pcm_from_24_lsb(bytes, 2) == values, "decode signed explicit vectors");
        check(virus::pcm_to_24_lsb(values, 2) == bytes, "encode signed explicit vectors");
        check(virus::pcm_to_24_lsb({2147483647, -2147483647 - 1, -1, 255, -255, 257}) ==
                  std::vector<std::uint8_t>{0xff, 0xff, 0x7f, 0, 0, 0x80,
                                            0xff, 0xff, 0xff, 0, 0, 0,
                                            0xff, 0xff, 0xff, 1, 0, 0},
              "discard low eight bits without rounding");
        check(virus::pcm_from_24_lsb({}).empty(), "empty decode");
        check(virus::pcm_to_24_lsb({}).empty(), "empty encode");
        rejects([] { virus::pcm_from_24_lsb({1}); }, "one-byte partial sample");
        rejects([] { virus::pcm_from_24_lsb({1, 2}); }, "two-byte partial sample");
        rejects([] { virus::pcm_from_24_lsb({1, 2, 3}, 2); }, "partial stereo frame");
        rejects([] { virus::pcm_from_24_lsb({}, 0); }, "zero decode channels");
        rejects([] { virus::pcm_from_24_lsb({}, std::numeric_limits<std::size_t>::max()); },
                "overflowing packed frame size");
        rejects([] { virus::pcm_to_24_lsb({1}, 2); }, "partial encode stereo frame");
        rejects([] { virus::pcm_to_24_lsb({}, 0); }, "zero encode channels");
        std::cout << "All transport-core tests passed (offline; no USB I/O).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
