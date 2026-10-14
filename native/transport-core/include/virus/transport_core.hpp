#ifndef VIRUS_TRANSPORT_CORE_HPP
#define VIRUS_TRANSPORT_CORE_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace virus {

// Serialized USB setup packet and its separate data stage. This type does not
// represent permission to submit the request, and no submission API is included.
struct SampleFrequencyRequest {
    std::array<std::uint8_t, 8> setup;
    std::array<std::uint8_t, 3> payload;
};

// Offline candidate reconstructed from one driver method. Accepts only candidate
// rates 44100/48000 and valid nonzero USB endpoint addresses (including direction).
// Throws std::invalid_argument on invalid input.
SampleFrequencyRequest make_sample_frequency_request(std::uint32_t endpoint_address,
                                                     std::uint32_t sample_rate);

// Bounded conversions allocate nothing and never throw. Invalid input leaves the
// entire destination unchanged. Counts are bytes for packed buffers and samples
// for int32 buffers. The caller provides valid storage for each declared count;
// int32 storage must have normal int32 alignment. Packed bytes may be unaligned.
// Buffers must not overlap. Empty buffers may use null pointers.
enum class BufferStatus { ok, invalid_argument, size_overflow, incomplete_frame,
                          insufficient_capacity, overlapping_buffers };
enum class PcmFormat : std::uint8_t { signed_16_le = 2, signed_24_le = 3 };
BufferStatus pcm_decode(PcmFormat format, const std::uint8_t* packed,
                        std::size_t byte_count, std::size_t channels,
                        std::int32_t* output, std::size_t output_samples) noexcept;
BufferStatus pcm_encode(PcmFormat format, const std::int32_t* samples,
                        std::size_t sample_count, std::size_t channels,
                        std::uint8_t* output, std::size_t output_bytes) noexcept;

// Allocating convenience wrappers for offline use only. Invalid input throws.
std::vector<std::int32_t> pcm_from_16_lsb(const std::vector<std::uint8_t>& bytes,
                                       std::size_t channels = 1);
std::vector<std::uint8_t> pcm_to_16_lsb(const std::vector<std::int32_t>& samples,
                                     std::size_t channels = 1);
std::vector<std::int32_t> pcm_from_24_lsb(const std::vector<std::uint8_t>& bytes,
                                       std::size_t channels = 1);
std::vector<std::uint8_t> pcm_to_24_lsb(const std::vector<std::int32_t>& samples,
                                     std::size_t channels = 1);

// Host-side design, NOT recovered device clock/feedback behavior. Explicit
// endpoint capacity must come from later descriptor/alternate-setting discovery.
struct AudioPacketConfig {
    std::uint32_t sample_rate;
    std::uint32_t intervals_per_second;
    std::size_t channels;
    PcmFormat format;
    std::size_t max_packet_bytes;
};
struct AudioPacketBounds {
    std::size_t bytes_per_frame;
    std::size_t maximum_frames;
    std::size_t maximum_bytes;
};
BufferStatus validate_audio_packet_config(const AudioPacketConfig& config,
                                          AudioPacketBounds& bounds) noexcept;

// Nominal floor/carry schedule. Exact integer totals with no floating point drift.
// No USB scheduling, device feedback, timestamps, or clock recovery implemented.
// Construction validates configuration and may throw; next_frames never allocates.
class NominalPacketSchedule {
public:
    explicit NominalPacketSchedule(const AudioPacketConfig& config);
    std::size_t next_frames() noexcept;
    void reset() noexcept { remainder_ = 0; }
private:
    std::uint32_t rate_;
    std::uint32_t intervals_;
    std::uint32_t remainder_ = 0;
};

enum class QueueStatus { ok, invalid_argument, insufficient_capacity,
                         overrun, underrun };
// Fixed-capacity SPSC queue of complete interleaved int32 frames. Constructor
// allocates once; push/pop allocate nothing. Exactly one producer calls push and
// one consumer calls pop. No concurrent destruction. Failure performs no transfer;
// underrun does not write silence (the host chooses its underrun policy).
// Capacity must be a power of two. Atomics are required to be lock-free.
class AudioFrameQueue {
public:
    AudioFrameQueue(std::size_t capacity_frames, std::size_t channels);
    AudioFrameQueue(const AudioFrameQueue&) = delete;
    AudioFrameQueue& operator=(const AudioFrameQueue&) = delete;
    QueueStatus push(const std::int32_t* input, std::size_t frames,
                     std::size_t input_samples) noexcept;
    QueueStatus pop(std::int32_t* output, std::size_t frames,
                    std::size_t output_samples) noexcept;
    std::size_t channels() const noexcept { return channels_; }
    std::size_t capacity_frames() const noexcept { return capacity_; }
private:
    const std::size_t capacity_;
    const std::size_t channels_;
    std::vector<std::int32_t> storage_;
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
};

}  // namespace virus
#endif
