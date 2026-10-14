#include "virus/transport_core.hpp"
#include "virus/usb_descriptors.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

// Entirely synthetic integration fixture. No USB framework or hardware access.
static void check(bool value) {
    if (!value) throw std::runtime_error("offline audio pipeline assertion failed");
}
static std::int32_t sample(std::size_t frame, std::size_t channel,
                           virus::PcmFormat format) {
    const auto index = frame * 17 + channel * 101;
    if (format == virus::PcmFormat::signed_16_le)
        return (static_cast<std::int32_t>(index % 60001) - 30000) * 65536;
    return (static_cast<std::int32_t>(index % 16000001) - 8000000) * 256;
}
static std::size_t run(std::uint32_t rate, std::uint32_t intervals,
                       std::size_t channels, virus::PcmFormat format) {
    // One invented USB2 configuration with inactive alt0 and isochronous alt1.
    // No real Virus interface/endpoint values or class-specific formats asserted.
    const unsigned capacity = intervals == 1000 ? 900 : 128;
    const std::vector<std::uint8_t> raw{
        9,2,34,0,1,1,0,0x80,50, 9,4,0,0,0,1,2,0,0,
        9,4,0,1,1,1,2,0,0, 7,5,0x81,0x05,
        static_cast<std::uint8_t>(capacity & 255),
        static_cast<std::uint8_t>(capacity >> 8),1};
    const auto config = virus::usb::parse_configuration(raw);
    const auto& endpoint = config.alternates.at(1).endpoints.at(0);
    const auto speed = intervals == 1000 ? virus::usb::Speed::full : virus::usb::Speed::high;
    const auto bytes = endpoint.bytes_per_service_interval(speed);
    const auto duration = endpoint.service_interval_ns(speed);
    check(bytes.has_value() && duration.has_value());
    check(1000000000ULL / *duration == intervals);
    const virus::AudioPacketConfig packet{rate, intervals, channels, format, *bytes};
    virus::AudioPacketBounds bounds{};
    check(virus::validate_audio_packet_config(packet, bounds) == virus::BufferStatus::ok);
    virus::NominalPacketSchedule schedule(packet);
    virus::AudioFrameQueue queue(512, channels);
    // Preallocate all storage before the simulated callbacks.
    std::array<std::int32_t, 768> input{}, decoded{}, host{};
    std::array<std::uint8_t, 900> packed{};
    std::size_t produced = 0, consumed = 0, queued = 0;
    const auto width = static_cast<std::size_t>(format);
    auto drain = [&](std::size_t frames) {
        check(queue.pop(host.data(), frames, host.size()) == virus::QueueStatus::ok);
        for (std::size_t f = 0; f < frames; ++f)
            for (std::size_t c = 0; c < channels; ++c)
                check(host[f * channels + c] == sample(consumed + f, c, format));
        consumed += frames;
        queued -= frames;
    };
    for (std::uint32_t tick = 0; tick < intervals; ++tick) {
        const auto frames = schedule.next_frames();
        const auto count = frames * channels;
        const auto byte_count = count * width;
        check(byte_count <= *bytes && count <= input.size());
        for (std::size_t f = 0; f < frames; ++f)
            for (std::size_t c = 0; c < channels; ++c)
                input[f * channels + c] = sample(produced + f, c, format);
        check(virus::pcm_encode(format, input.data(), count, channels,
                               packed.data(), packed.size()) == virus::BufferStatus::ok);
        check(virus::pcm_decode(format, packed.data(), byte_count, channels,
                               decoded.data(), decoded.size()) == virus::BufferStatus::ok);
        check(queue.push(decoded.data(), frames, decoded.size()) == virus::QueueStatus::ok);
        produced += frames;
        queued += frames;
        // Host consumes 128-frame blocks, distinct from USB packet boundaries.
        while (queued >= 128) drain(128);
    }
    if (queued) drain(queued);
    check(produced == rate && consumed == rate);
    check(queue.pop(host.data(), 1, host.size()) == virus::QueueStatus::underrun);
    return consumed * channels;
}
int main() {
    try {
        std::size_t cases = 0, samples = 0;
        for (auto rate : {44100u, 48000u})
            for (auto intervals : {1000u, 8000u})
                for (std::size_t channels : {2u, 6u})
                    for (auto format : {virus::PcmFormat::signed_16_le, virus::PcmFormat::signed_24_le}) {
                        samples += run(rate, intervals, channels, format);
                        ++cases;
                    }
        std::cout << "{\"mode\":\"synthetic_offline\",\"cases_passed\":" << cases
                  << ",\"samples_verified\":" << samples
                  << ",\"hardware_accessed\":false,\"live_audio_verified\":false}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
