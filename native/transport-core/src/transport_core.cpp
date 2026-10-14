#include "virus/transport_core.hpp"

#include <limits>
#include <stdexcept>

namespace virus {

SampleFrequencyRequest make_sample_frequency_request(std::uint32_t endpoint_address,
                                                     std::uint32_t sample_rate) {
    if ((endpoint_address & ~std::uint32_t{0x8f}) != 0 ||
        (endpoint_address & 0x0f) == 0) {
        throw std::invalid_argument("endpoint must be nonzero with no reserved bits");
    }
    if (sample_rate != 44100 && sample_rate != 48000) {
        throw std::invalid_argument("only 44100 and 48000 are candidate sample rates");
    }
    return {{0x22, 0x01, 0x00, 0x01, static_cast<std::uint8_t>(endpoint_address),
             0x00, 0x03, 0x00},
            {static_cast<std::uint8_t>(sample_rate & 0xff),
             static_cast<std::uint8_t>((sample_rate >> 8) & 0xff),
             static_cast<std::uint8_t>((sample_rate >> 16) & 0xff)}};
}

namespace {
constexpr auto limit = std::numeric_limits<std::size_t>::max();
std::size_t width(PcmFormat f) noexcept {
    return f == PcmFormat::signed_16_le ? 2 : f == PcmFormat::signed_24_le ? 3 : 0;
}
bool overlaps(const void* a, std::size_t an, const void* b, std::size_t bn) noexcept {
    if (!an || !bn) return false;
    const auto x = reinterpret_cast<std::uintptr_t>(a);
    const auto y = reinterpret_cast<std::uintptr_t>(b);
    // Subtraction avoids address-range addition overflow.
    return x <= y ? y - x < an : x - y < bn;
}
void require_ok(BufferStatus status) {
    if (status != BufferStatus::ok) throw std::invalid_argument("invalid PCM buffer");
}
std::vector<std::int32_t> decode_vector(const std::vector<std::uint8_t>& bytes,
                                       std::size_t channels, PcmFormat format) {
    const auto n = width(format);
    if (!channels || channels > limit / n || bytes.size() % (channels * n))
        throw std::invalid_argument("invalid PCM frame");
    std::vector<std::int32_t> result(bytes.size() / n);
    require_ok(pcm_decode(format, bytes.data(), bytes.size(), channels,
                          result.data(), result.size()));
    return result;
}
std::vector<std::uint8_t> encode_vector(const std::vector<std::int32_t>& samples,
                                       std::size_t channels, PcmFormat format) {
    const auto n = width(format);
    if (!channels || channels > limit / n || samples.size() % channels)
        throw std::invalid_argument("invalid PCM frame");
    if (samples.size() > limit / n) throw std::length_error("PCM size overflow");
    std::vector<std::uint8_t> result(samples.size() * n);
    require_ok(pcm_encode(format, samples.data(), samples.size(), channels,
                          result.data(), result.size()));
    return result;
}
} // namespace

BufferStatus pcm_decode(PcmFormat format, const std::uint8_t* packed,
                        std::size_t byte_count, std::size_t channels,
                        std::int32_t* output, std::size_t output_samples) noexcept {
    const auto n = width(format);
    if (!n || !channels) return BufferStatus::invalid_argument;
    if (channels > limit / n) return BufferStatus::size_overflow;
    if (byte_count % (channels * n)) return BufferStatus::incomplete_frame;
    const auto count = byte_count / n;
    if (count > limit / sizeof(std::int32_t)) return BufferStatus::size_overflow;
    if (count > output_samples) return BufferStatus::insufficient_capacity;
    if (count && (!packed || !output)) return BufferStatus::invalid_argument;
    if (overlaps(packed, byte_count, output, count * sizeof(std::int32_t)))
        return BufferStatus::overlapping_buffers;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint32_t bits = 0;
        for (std::size_t j = 0; j < n; ++j)
            bits |= std::uint32_t{packed[i * n + j]} << ((4 - n + j) * 8);
        const std::int64_t value = (bits & 0x80000000u)
            ? std::int64_t{bits} - 0x100000000LL : std::int64_t{bits};
        output[i] = static_cast<std::int32_t>(value);
    }
    return BufferStatus::ok;
}

BufferStatus pcm_encode(PcmFormat format, const std::int32_t* samples,
                        std::size_t sample_count, std::size_t channels,
                        std::uint8_t* output, std::size_t output_bytes) noexcept {
    const auto n = width(format);
    if (!n || !channels) return BufferStatus::invalid_argument;
    if (channels > limit / n || sample_count > limit / n ||
        sample_count > limit / sizeof(std::int32_t)) return BufferStatus::size_overflow;
    if (sample_count % channels) return BufferStatus::incomplete_frame;
    const auto bytes = sample_count * n;
    if (bytes > output_bytes) return BufferStatus::insufficient_capacity;
    if (sample_count && (!samples || !output)) return BufferStatus::invalid_argument;
    if (overlaps(samples, sample_count * sizeof(std::int32_t), output, bytes))
        return BufferStatus::overlapping_buffers;
    for (std::size_t i = 0; i < sample_count; ++i) {
        const auto bits = static_cast<std::uint32_t>(samples[i]);
        for (std::size_t j = 0; j < n; ++j)
            output[i * n + j] = static_cast<std::uint8_t>(bits >> ((4 - n + j) * 8));
    }
    return BufferStatus::ok;
}

std::vector<std::int32_t> pcm_from_16_lsb(const std::vector<std::uint8_t>& bytes,
                                       std::size_t channels) {
    return decode_vector(bytes, channels, PcmFormat::signed_16_le);
}
std::vector<std::uint8_t> pcm_to_16_lsb(const std::vector<std::int32_t>& samples,
                                     std::size_t channels) {
    return encode_vector(samples, channels, PcmFormat::signed_16_le);
}
std::vector<std::int32_t> pcm_from_24_lsb(const std::vector<std::uint8_t>& bytes,
                                       std::size_t channels) {
    return decode_vector(bytes, channels, PcmFormat::signed_24_le);
}
std::vector<std::uint8_t> pcm_to_24_lsb(const std::vector<std::int32_t>& samples,
                                     std::size_t channels) {
    return encode_vector(samples, channels, PcmFormat::signed_24_le);
}

BufferStatus validate_audio_packet_config(const AudioPacketConfig& config,
                                          AudioPacketBounds& bounds) noexcept {
    const auto n = width(config.format);
    if (!n || !config.channels ||
        (config.sample_rate != 44100 && config.sample_rate != 48000) ||
        (config.intervals_per_second != 1000 && config.intervals_per_second != 8000))
        return BufferStatus::invalid_argument;
    if (config.channels > limit / n) return BufferStatus::size_overflow;
    const auto frame_bytes = config.channels * n;
    const auto frames = (config.sample_rate + config.intervals_per_second - 1) /
                        config.intervals_per_second;
    if (frames > limit / frame_bytes) return BufferStatus::size_overflow;
    const auto bytes = frame_bytes * frames;
    if (bytes > config.max_packet_bytes) return BufferStatus::insufficient_capacity;
    bounds = {frame_bytes, frames, bytes};
    return BufferStatus::ok;
}
NominalPacketSchedule::NominalPacketSchedule(const AudioPacketConfig& config)
    : rate_(config.sample_rate), intervals_(config.intervals_per_second) {
    AudioPacketBounds bounds{};
    require_ok(validate_audio_packet_config(config, bounds));
}
std::size_t NominalPacketSchedule::next_frames() noexcept {
    const auto total = rate_ + remainder_;
    remainder_ = total % intervals_;
    return total / intervals_;
}

namespace {
std::size_t queue_samples(std::size_t capacity, std::size_t channels) {
    if (!capacity || (capacity & (capacity - 1)) || !channels ||
        capacity > limit / 2 || capacity > limit / channels ||
        capacity * channels > limit / sizeof(std::int32_t))
        throw std::invalid_argument("invalid audio queue capacity/channels");
    return capacity * channels;
}
}
AudioFrameQueue::AudioFrameQueue(std::size_t capacity_frames, std::size_t channels)
    : capacity_(capacity_frames), channels_(channels),
      storage_(queue_samples(capacity_frames, channels)) {
    static_assert(std::atomic<std::size_t>::is_always_lock_free,
                  "audio queue requires lock-free size_t atomics");
}
QueueStatus AudioFrameQueue::push(const std::int32_t* input, std::size_t frames,
                                  std::size_t input_samples) noexcept {
    if (frames > limit / channels_ || (frames && !input))
        return QueueStatus::invalid_argument;
    if (frames * channels_ > input_samples) return QueueStatus::insufficient_capacity;
    const auto w = write_.load(std::memory_order_relaxed);
    const auto r = read_.load(std::memory_order_acquire);
    if (frames > capacity_ - (w - r)) return QueueStatus::overrun;
    for (std::size_t f = 0; f < frames; ++f)
        for (std::size_t c = 0; c < channels_; ++c)
            storage_[((w + f) & (capacity_ - 1)) * channels_ + c] = input[f * channels_ + c];
    write_.store(w + frames, std::memory_order_release);
    return QueueStatus::ok;
}
QueueStatus AudioFrameQueue::pop(std::int32_t* output, std::size_t frames,
                                 std::size_t output_samples) noexcept {
    if (frames > limit / channels_ || (frames && !output))
        return QueueStatus::invalid_argument;
    if (frames * channels_ > output_samples) return QueueStatus::insufficient_capacity;
    const auto r = read_.load(std::memory_order_relaxed);
    const auto w = write_.load(std::memory_order_acquire);
    if (frames > w - r) return QueueStatus::underrun;
    for (std::size_t f = 0; f < frames; ++f)
        for (std::size_t c = 0; c < channels_; ++c)
            output[f * channels_ + c] = storage_[((r + f) & (capacity_ - 1)) * channels_ + c];
    read_.store(r + frames, std::memory_order_release);
    return QueueStatus::ok;
}
}  // namespace virus
