#include "virus/audio_engine.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <stdexcept>

namespace virus::audio {
namespace {
std::size_t dir(Direction d) { return d == Direction::capture ? 0 : 1; }
void validate(const StreamConfig& s, usb::Speed speed, bool input,
              const AudioFrameQueue& queue) {
    const auto capacity = s.endpoint.bytes_per_service_interval(speed);
    const auto interval = s.endpoint.service_interval_ns(speed);
    AudioPacketBounds bounds{};
    if (s.endpoint.transfer_type() != 1 || s.endpoint.usage_type() == 1 ||
        s.endpoint.usage_type() == 3 || !(s.endpoint.address & 15) ||
        (s.endpoint.address & 0x70) || bool(s.endpoint.address & 0x80) != input ||
        !capacity || !interval || s.endpoint.interval != 1 ||
        s.audio.max_packet_bytes != *capacity ||
        s.audio.intervals_per_second != (speed == usb::Speed::high ? 8000u : 1000u) ||
        validate_audio_packet_config(s.audio, bounds) != BufferStatus::ok ||
        queue.channels() != s.audio.channels || queue.capacity_frames() < bounds.maximum_frames)
        throw std::invalid_argument("unsupported or inconsistent audio endpoint/queue");
}
}
Engine::Engine(Config config, Backend& backend, AudioFrameQueue& capture,
               AudioFrameQueue& playback)
    : config_(config), backend_(backend), capture_(capture), playback_(playback),
      playback_schedule_(config.playback.audio) {
    if ((config.speed != usb::Speed::full && config.speed != usb::Speed::high) ||
        !config.requests_per_direction || config.requests_per_direction > max_requests / 2 ||
        !config.packets_per_request || config.packets_per_request > max_packets ||
        config.capture.audio.sample_rate != config.playback.audio.sample_rate ||
        &capture == &playback)
        throw std::invalid_argument("unsupported audio engine configuration");
    validate(config.capture, config.speed, true, capture);
    validate(config.playback, config.speed, false, playback);
    // One packet scratch area large enough for any complete PCM frame in either endpoint.
    scratch_.resize(std::max(config.capture.audio.max_packet_bytes /
                            static_cast<unsigned>(config.capture.audio.format),
                            config.playback.audio.max_packet_bytes /
                            static_cast<unsigned>(config.playback.audio.format)));
    for (std::size_t i = 0; i < 2 * config.requests_per_direction; ++i) {
        auto& s = slots_[i];
        const bool input = i < config.requests_per_direction;
        const auto& c = input ? config.capture : config.playback;
        s.bytes.resize(config.packets_per_request * c.audio.max_packet_bytes);
        s.request.direction = input ? Direction::capture : Direction::playback;
        s.request.endpoint = c.endpoint.address;
        s.request.interval_ns = *c.endpoint.service_interval_ns(config.speed);
        s.request.bytes = s.bytes.data();
        s.request.capacity = s.bytes.size();
        s.request.packet_count = config.packets_per_request;
    }
}
Engine::~Engine() { if (outstanding()) std::terminate(); }
std::size_t Engine::outstanding() const noexcept {
    std::size_t count = 0;
    for (const auto& s : slots_) if (s.ownership != Ownership::free) ++count;
    return count;
}
bool Engine::start(std::uint64_t first_service) noexcept {
    if (state_ != State::idle || generation_ == UINT64_MAX) return false;
    ++generation_;
    failed_ = false;
    next_service_.fill(first_service);
    retire_service_.fill(first_service);
    playback_schedule_.reset();
    stats_.capture_timestamp_valid = false;
    state_ = State::running;
    pump();
    return !failed_;
}
void Engine::fault() noexcept { failed_ = true; stop(); }
void Engine::stop() noexcept {
    if (state_ == State::idle || state_ == State::draining) return;
    state_ = State::draining;
    stats_.capture_timestamp_valid = false;
    for (auto& s : slots_) {
        if (s.ownership == Ownership::submitted) backend_.cancel(s.request.token);
        else if (s.ownership == Ownership::completed) s.ownership = Ownership::free;
    }
    if (!outstanding()) state_ = State::idle;
}
void Engine::pump() noexcept {
    if (state_ != State::running) return;
    for (std::size_t i = 0; i < 2 * config_.requests_per_direction; ++i) {
        auto& s = slots_[i];
        if (s.ownership != Ownership::free) continue;
        auto& r = s.request;
        const auto d = dir(r.direction);
        const auto& c = d == 0 ? config_.capture.audio : config_.playback.audio;
        if (serial_ == UINT64_MAX || next_service_[d] > UINT64_MAX - r.packet_count) {
            fault(); return;
        }
        r.token = {generation_, ++serial_, i};
        r.first_service = next_service_[d];
        next_service_[d] += r.packet_count;
        for (std::size_t p = 0; p < r.packet_count; ++p) {
            auto& packet = r.packets[p];
            packet.offset = p * c.max_packet_bytes;
            packet.requested_bytes = c.max_packet_bytes;
            if (d == 1) {
                const auto frames = playback_schedule_.next_frames();
                const auto samples = frames * c.channels;
                packet.requested_bytes = samples * static_cast<unsigned>(c.format);
                if (playback_.pop(scratch_.data(), frames, scratch_.size()) != QueueStatus::ok) {
                    std::fill_n(scratch_.data(), samples, 0);
                    ++stats_.underrun_packets;
                }
                const auto status = pcm_encode(c.format, scratch_.data(), samples, c.channels,
                    s.bytes.data() + packet.offset, packet.requested_bytes);
                if (status != BufferStatus::ok) { fault(); return; }
            }
        }
        s.ownership = Ownership::submitted;
        if (backend_.submit(r) == SubmitStatus::rejected) {
            s.ownership = Ownership::free;
            ++stats_.submission_errors;
            fault(); return;
        }
        ++stats_.submissions;
    }
}
void Engine::complete(const Completion& completion) noexcept {
    if (completion.token.slot >= slots_.size()) { ++stats_.stale_completions; return; }
    auto& s = slots_[completion.token.slot];
    if (s.ownership != Ownership::submitted || !(s.request.token == completion.token)) {
        ++stats_.stale_completions; return;
    }
    // Receipt of a terminal callback releases backend ownership even if malformed.
    s.completion = completion;
    s.ownership = Ownership::completed;
    if (completion.packet_count != s.request.packet_count) {
        ++stats_.malformed_completions;
        fault();
    }
    if (state_ != State::running) {
        s.ownership = Ownership::free;
        if (!outstanding()) state_ = State::idle;
        return;
    }
    retire();
}
void Engine::retire() noexcept {
    // USB callbacks may arrive out of order. Hold completed buffers until every
    // preceding request in that direction has retired, preventing audio reordering.
    bool progress;
    do {
        progress = false;
        for (auto& s : slots_) {
            const auto d = dir(s.request.direction);
            if (s.ownership != Ownership::completed ||
                s.request.first_service != retire_service_[d]) continue;
            process(s);
            if (state_ != State::running) return;
            retire_service_[d] += s.request.packet_count;
            s.ownership = Ownership::free;
            progress = true;
        }
    } while (progress);
}
void Engine::process(Slot& s) noexcept {
    const bool input = s.request.direction == Direction::capture;
    const auto& c = input ? config_.capture.audio : config_.playback.audio;
    const auto frame_bytes = c.channels * static_cast<unsigned>(c.format);
    for (std::size_t i = 0; i < s.request.packet_count; ++i) {
        const auto& result = s.completion.packets[i];
        const auto& packet = s.request.packets[i];
        if (result.actual_bytes > packet.requested_bytes || result.actual_bytes % frame_bytes) {
            ++stats_.malformed_completions;
            fault(); return;
        }
        if (result.status != PacketStatus::success) {
            ++stats_.packet_errors;
            ++stats_.discontinuities;
            if (input) stats_.capture_timestamp_valid = false;
            if (result.status == PacketStatus::disconnected || result.status == PacketStatus::stalled) {
                fault(); return;
            }
            continue; // Error payload is never decoded, even if actual_bytes > 0.
        }
        const auto frames = result.actual_bytes / frame_bytes;
        if (!input) {
            if (result.actual_bytes < packet.requested_bytes) {
                ++stats_.partial_packets;
                ++stats_.discontinuities;
            }
            stats_.playback_frames += frames;
            continue;
        }
        if (!frames) { ++stats_.discontinuities; stats_.capture_timestamp_valid = false; continue; }
        if (pcm_decode(c.format, s.bytes.data() + packet.offset, result.actual_bytes,
                       c.channels, scratch_.data(), scratch_.size()) != BufferStatus::ok) {
            ++stats_.malformed_completions;
            fault(); return;
        }
        if (capture_.push(scratch_.data(), frames, scratch_.size()) != QueueStatus::ok) {
            ++stats_.overrun_packets;
            ++stats_.discontinuities;
            stats_.capture_timestamp_valid = false;
        } else {
            stats_.capture_frames += frames;
            stats_.capture_timestamp_valid = result.timestamp_valid;
            stats_.last_capture_host_ns = result.host_time_ns;
            stats_.last_capture_service = s.request.first_service + i;
        }
    }
}

RecoveredFrameObserver::RecoveredFrameObserver(std::uint32_t rate, usb::Speed speed) {
    if ((rate != 44100 && rate != 48000) ||
        (speed != usb::Speed::full && speed != usb::Speed::high))
        throw std::invalid_argument("observer scope is 44.1/48k USB2 FS/HS only");
    const bool high = speed == usb::Speed::high;
    length_ = high ? 128 : 16;
    const auto nominal = static_cast<std::int32_t>(rate / (high ? 8000 : 1000));
    minimum_ = nominal - (high ? 1 : 2);
    maximum_ = nominal + (high ? 1 : 2);
    for (std::size_t i = 0; i < length_; ++i) {
        auto value = static_cast<std::uint32_t>(nominal);
        if (rate == 44100) value = high ? 5 + (i & 1) + (i % 80 == 0) : 44 + (i % 10 == 0);
        initial_[i] = learned_[i] = value;
    }
}
std::uint32_t RecoveredFrameObserver::observe(std::int32_t samples) noexcept {
    std::uint32_t value;
    if (samples < minimum_ || samples > maximum_) {
        ++errors_; synchronized_ = false; last_error_ = input_; value = initial_[input_];
    } else value = static_cast<std::uint32_t>(samples);
    learned_[input_] = value;
    input_ = (input_ + 1) % length_;
    if (!synchronized_ && input_ == last_error_) { synchronized_ = true; errors_ = 0; }
    return value;
}
std::uint32_t RecoveredFrameObserver::next_output_frames() noexcept {
    const auto value = learned_[output_];
    output_ = (output_ + 1) % length_;
    return value;
}
} // namespace virus::audio
