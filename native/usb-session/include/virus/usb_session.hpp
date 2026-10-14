#pragma once
#include "virus/audio_engine.hpp"
#include <array>
#include <stdexcept>

namespace virus::session {
struct Selection { std::uint8_t interface_number, alternate_setting, endpoint; };
struct InterfacePlan { std::uint8_t number, alternate; };
struct RatePlan { std::uint8_t interface_number, endpoint; };
class Plan {
public:
    const audio::Config& audio_config() const noexcept { return audio; }
    std::uint8_t configuration_value() const noexcept { return configuration; }
    std::size_t rate_control_count() const noexcept { return rate_count; }
    std::size_t interface_control_count() const noexcept { return interface_count; }
private:
    Plan() = default;
    friend class Session;
    friend Plan make_plan(const usb::Configuration&, usb::Speed, Selection, Selection, std::uint32_t);
    std::uint8_t configuration;
    audio::Config audio;
    std::array<InterfacePlan, 2> interfaces{};
    std::size_t interface_count = 0;
    std::array<RatePlan, 2> rates{};
    std::size_t rate_count = 0;
};
// Fail closed: only UAC1 Type-I PCM16/24, explicit selections, 44.1/48k,
// USB2 FS/HS isochronous data bInterval=1. Format and rate come from
// descriptors. No vendor-specific format inference, endpoint auto-selection,
// or inference from VID/PID. Throws invalid_argument before any USB action.
Plan make_plan(const usb::Configuration&, usb::Speed, Selection capture,
               Selection playback, std::uint32_t sample_rate);

enum class State { idle, preparing, ready, running, draining, failed, uncertain };
enum class Error { none, configuration, open, alternate_read, alternate_select,
                   rate_read, rate_write, start, rollback };
// Control executor only. The concrete owner must enforce registry device identity,
// common serial queue and exclusive interface ownership. Failed mutations may
// have changed device state; rollback is attempted even on a failed write/select.
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool current_configuration(std::uint8_t&) noexcept = 0;
    // Failed open retains nothing. Successful open must be closed exactly once.
    virtual bool open_interface(std::uint8_t number) noexcept = 0;
    virtual bool current_alternate(std::uint8_t number, std::uint8_t&) noexcept = 0;
    virtual bool select_alternate(std::uint8_t number, std::uint8_t alternate) noexcept = 0;
    virtual bool read_rate(RatePlan, std::uint32_t&) noexcept = 0;
    virtual bool write_rate(RatePlan, std::uint32_t) noexcept = 0;
    virtual void close_interface(std::uint8_t number) noexcept = 0;
    // Receives validated immutable config. False may still leave outstanding I/O.
    // Recreate pipes AFTER alternate selection. Queue and pipes survive drain.
    virtual bool start_io(const audio::Config&, std::uint64_t first_service) noexcept = 0;
    virtual void stop_io() noexcept = 0;
    virtual bool io_drained() const noexcept = 0;
};
class Session {
public:
    Session(Plan, Backend&);
    // Caller must stop and poll until not draining before destruction.
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    bool prepare() noexcept;
    bool start(std::uint64_t first_service) noexcept;
    void stop() noexcept;
    void poll() noexcept;
    State state() const noexcept { return state_; }
    Error error() const noexcept { return error_; }
    bool rollback_failed() const noexcept { return rollback_failed_; }
private:
    void rollback() noexcept;
    bool fail(Error) noexcept;
    const Plan plan_;
    Backend& backend_;
    State state_ = State::idle;
    Error error_ = Error::none;
    bool rollback_failed_ = false, io_started_ = false;
    std::size_t opened_ = 0, alternates_changed_ = 0, rates_changed_ = 0;
    std::array<std::uint8_t, 2> original_alternate_{};
    std::array<std::uint32_t, 2> original_rate_{};
};
} // namespace virus::session
