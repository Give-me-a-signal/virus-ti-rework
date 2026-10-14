#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace virus::midi {

enum class Error { none, stray_data, unexpected_end, interrupted, undefined_status,
                   sysex_overflow, truncated, invalid_buffer };
enum class Kind { message, error };
struct Event {
    Kind kind;
    const std::uint8_t* bytes;
    std::size_t size;
    std::uint64_t timestamp;
    Error error;
};
using Sink = void (*)(void*, const Event&);

// No allocation or locks. One owner per parser. The callback must not reenter
// the parser; byte views expire on callback return. Timestamp units are chosen
// by the caller and refer to the first actual byte, including running status.
template<std::size_t SysexCapacity = 2048>
class StreamParser {
    static_assert(SysexCapacity >= 2, "SysEx capacity includes F0 and F7");
public:
    explicit StreamParser(Sink sink, void* context = nullptr) noexcept
        : sink_(sink), context_(context) {}

    void reset() noexcept {
        used_ = short_used_ = needed_ = 0;
        running_ = 0;
        sysex_ = dropping_ = false;
    }
    void finish(std::uint64_t timestamp = 0) {
        if ((sysex_ && !dropping_) || short_used_) fail(Error::truncated, timestamp);
        reset();
    }
    void feed(const std::uint8_t* bytes, std::size_t size, std::uint64_t timestamp) {
        if (!bytes && size) { fail(Error::invalid_buffer, timestamp); return; }
        for (std::size_t i = 0; i < size; ++i) push(bytes[i], timestamp);
    }
    void push(std::uint8_t byte, std::uint64_t timestamp) {
        // Real-time is independent of running status and SysEx, even while
        // discarding an oversized SysEx. Reserved realtime bytes are diagnosed.
        if (byte >= 0xf8) {
            if (byte == 0xf9 || byte == 0xfd) fail(Error::undefined_status, timestamp);
            else emit(&byte, 1, timestamp);
            return;
        }
        if (sysex_) {
            if (byte < 0x80 || byte == 0xf7) {
                if (!dropping_) {
                    if (used_ == SysexCapacity) {
                        dropping_ = true;
                        fail(Error::sysex_overflow, timestamp);
                    } else buffer_[used_++] = byte;
                }
                if (byte == 0xf7) {
                    if (!dropping_) emit(buffer_.data(), used_, start_);
                    sysex_ = dropping_ = false;
                    used_ = 0;
                }
                return;
            }
            if (!dropping_) fail(Error::interrupted, timestamp);
            sysex_ = dropping_ = false;
            used_ = 0;
            // Resynchronize at this new status; never emit a partial SysEx.
        }
        if (byte >= 0x80) {
            if (short_used_) fail(Error::interrupted, timestamp);
            short_used_ = needed_ = 0;
            if (byte < 0xf0) running_ = byte;
            else running_ = 0;
            if (byte == 0xf0) {
                sysex_ = true; buffer_[0] = byte; used_ = 1; start_ = timestamp;
            } else if (byte == 0xf7) fail(Error::unexpected_end, timestamp);
            else if (byte == 0xf4 || byte == 0xf5) fail(Error::undefined_status, timestamp);
            else if (byte == 0xf6) emit(&byte, 1, timestamp);
            else begin(byte, timestamp);
            return;
        }
        if (!short_used_) {
            if (!running_) { fail(Error::stray_data, timestamp); return; }
            begin(running_, timestamp);
        }
        short_[short_used_++] = byte;
        if (short_used_ == needed_) {
            emit(short_.data(), short_used_, start_);
            short_used_ = needed_ = 0;
        }
    }
private:
    void begin(std::uint8_t status, std::uint64_t timestamp) noexcept {
        short_[0] = status; short_used_ = 1; start_ = timestamp;
        needed_ = ((status & 0xf0) == 0xc0 || (status & 0xf0) == 0xd0 ||
                   status == 0xf1 || status == 0xf3) ? 2 : 3;
    }
    void emit(const std::uint8_t* bytes, std::size_t size, std::uint64_t timestamp) {
        if (sink_) sink_(context_, {Kind::message, bytes, size, timestamp, Error::none});
    }
    void fail(Error error, std::uint64_t timestamp) {
        if (sink_) sink_(context_, {Kind::error, nullptr, 0, timestamp, error});
    }
    Sink sink_;
    void* context_;
    std::array<std::uint8_t, SysexCapacity> buffer_{};
    std::array<std::uint8_t, 3> short_{};
    std::size_t used_ = 0, short_used_ = 0, needed_ = 0;
    std::uint64_t start_ = 0;
    std::uint8_t running_ = 0;
    bool sysex_ = false, dropping_ = false;
};

// XML's cc and pp templates contain status/controllerid/value. Channel is
// explicit MIDI channel 0..15; this API does not infer the Virus part mapping.
inline bool build_control_change(unsigned channel, unsigned controller,
                                 unsigned value, std::array<std::uint8_t, 3>& out) noexcept {
    if (channel > 15 || controller > 127 || value > 127) return false;
    out = {static_cast<std::uint8_t>(0xb0 | channel),
           static_cast<std::uint8_t>(controller), static_cast<std::uint8_t>(value)};
    return true;
}
inline bool build_poly_pressure(unsigned channel, unsigned controller,
                                unsigned value, std::array<std::uint8_t, 3>& out) noexcept {
    if (channel > 15 || controller > 127 || value > 127) return false;
    out = {static_cast<std::uint8_t>(0xa0 | channel),
           static_cast<std::uint8_t>(controller), static_cast<std::uint8_t>(value)};
    return true;
}

// Exact short SysEx structure from remotedevice.xml, decimal byte tags.
// Explicit raw device/part fields: no default, broadcast, or numbering claims.
// This is a declaration encoder, not permission to send internal/global writes.
enum class Bank : unsigned { C = 110, D = 111, A = 112, B = 113,
                             E = 114, F = 115, G = 116 };
inline bool build_declared_bank_write(Bank bank, unsigned raw_device_id,
                                      unsigned raw_part, unsigned controller,
                                      unsigned value, std::array<std::uint8_t, 11>& out) noexcept {
    const auto command = static_cast<unsigned>(bank);
    if (command < 110 || command > 116 || raw_device_id > 127 || raw_part > 127 ||
        controller > 127 || value > 127) return false;
    out = {0xf0, 0x00, 0x20, 0x33, 0x01, static_cast<std::uint8_t>(raw_device_id),
           static_cast<std::uint8_t>(command), static_cast<std::uint8_t>(raw_part),
           static_cast<std::uint8_t>(controller), static_cast<std::uint8_t>(value), 0xf7};
    return true;
}
} // namespace virus::midi
