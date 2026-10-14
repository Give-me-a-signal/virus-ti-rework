#pragma once
#include "virus/transport_core.hpp"
#include "virus/usb_descriptors.hpp"
#include <array>
#include <vector>

namespace virus::audio {
constexpr std::size_t max_requests = 8, max_packets = 8;
enum class Direction { capture, playback };
enum class State { idle, running, draining };
enum class PacketStatus { success, cancelled, missed, stalled, disconnected, error };
enum class SubmitStatus { accepted, rejected };
struct Token {
    std::uint64_t generation = 0, serial = 0;
    std::size_t slot = 0;
    bool operator==(const Token& r) const noexcept {
        return generation == r.generation && serial == r.serial && slot == r.slot;
    }
};
struct Packet { std::size_t offset = 0, requested_bytes = 0; };
struct Request {
    Token token;
    Direction direction{};
    std::uint8_t endpoint = 0;
    // Service-interval ordinal, NOT mach time, nanoseconds or an OS USB frame.
    // Adapter must provide a documented mapping to its bus scheduling domain.
    std::uint64_t first_service = 0;
    std::uint64_t interval_ns = 0;
    std::uint8_t* bytes = nullptr;
    std::size_t capacity = 0, packet_count = 0;
    std::array<Packet, max_packets> packets{};
};
struct PacketResult {
    PacketStatus status = PacketStatus::error;
    std::size_t actual_bytes = 0;
    // Optional adapter-normalized monotonic host nanoseconds; never synthesized.
    bool timestamp_valid = false;
    std::uint64_t host_time_ns = 0;
};
struct Completion {
    Token token;
    std::size_t packet_count = 0;
    std::array<PacketResult, max_packets> packets{};
};
// All methods and Engine calls execute on ONE serialized control executor.
// No inline/reentrant completions. Accepted submit retains bytes/request until
// exactly one terminal completion, including after cancel. Rejected submit must
// retain nothing and produce no completion. cancel does NOT release ownership.
// Backend and queues outlive Engine. A real adapter must arrange callback drain.
class Backend {
public:
    virtual ~Backend() = default;
    virtual SubmitStatus submit(const Request&) noexcept = 0;
    virtual void cancel(Token) noexcept = 0;
};
struct StreamConfig { usb::Endpoint endpoint; AudioPacketConfig audio; };
struct Config {
    usb::Speed speed;
    StreamConfig capture, playback;
    std::size_t requests_per_direction = 2, packets_per_request = 8;
};
struct Stats {
    std::uint64_t submissions = 0, submission_errors = 0, stale_completions = 0;
    std::uint64_t malformed_completions = 0, packet_errors = 0, partial_packets = 0;
    std::uint64_t capture_frames = 0, playback_frames = 0;
    std::uint64_t overrun_packets = 0, underrun_packets = 0, discontinuities = 0;
    bool capture_timestamp_valid = false;
    std::uint64_t last_capture_host_ns = 0, last_capture_service = 0;
};
class Engine {
public:
    // Validates descriptors and allocates all storage. No hardware action.
    Engine(Config, Backend&, AudioFrameQueue& capture, AudioFrameQueue& playback);
    // Destruction with outstanding requests terminates: freeing live DMA storage
    // is never safe. stop(), deliver terminal callbacks, verify idle(), then destroy.
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    bool start(std::uint64_t first_service) noexcept;
    void pump() noexcept;
    void stop() noexcept;
    void complete(const Completion&) noexcept;
    State state() const noexcept { return state_; }
    bool failed() const noexcept { return failed_; }
    std::size_t outstanding() const noexcept;
    const Stats& stats() const noexcept { return stats_; }
private:
    enum class Ownership { free, submitted, completed };
    struct Slot {
        Ownership ownership = Ownership::free;
        Request request;
        Completion completion;
        std::vector<std::uint8_t> bytes;
    };
    void retire() noexcept;
    void process(Slot&) noexcept;
    void fault() noexcept;
    Config config_;
    Backend& backend_;
    AudioFrameQueue& capture_;
    AudioFrameQueue& playback_;
    NominalPacketSchedule playback_schedule_;
    std::array<Slot, max_requests> slots_;
    std::vector<std::int32_t> scratch_;
    std::array<std::uint64_t, 2> next_service_{}, retire_service_{};
    std::uint64_t generation_ = 0, serial_ = 0;
    State state_ = State::idle;
    bool failed_ = false;
    Stats stats_{};
};

// Static behavioral reconstruction of init/onRtsFrame/initWrite_Std, limited to
// 44.1/48 kHz FS/HS and generic tolerance (not device-specific VID/PID override).
// Independent from Engine: repeating this table is NOT verified clock recovery.
class RecoveredFrameObserver {
public:
    RecoveredFrameObserver(std::uint32_t sample_rate, usb::Speed);
    std::uint32_t observe(std::int32_t samples) noexcept;
    std::uint32_t next_output_frames() noexcept;
    bool synchronized() const noexcept { return synchronized_; }
    std::uint32_t errors_since_sync() const noexcept { return errors_; }
    std::size_t length() const noexcept { return length_; }
private:
    std::array<std::uint32_t,128> initial_{}, learned_{};
    std::size_t length_, input_ = 0, output_ = 0, last_error_ = 0;
    std::int32_t minimum_, maximum_;
    std::uint32_t errors_ = 0;
    bool synchronized_ = false;
};
} // namespace virus::audio
