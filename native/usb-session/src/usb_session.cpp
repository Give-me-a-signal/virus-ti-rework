#include "virus/usb_session.hpp"
#include <exception>
#include <utility>

namespace virus::session {
namespace {
[[noreturn]] void reject(const char* reason) { throw std::invalid_argument(reason); }
std::uint32_t le24(const std::vector<std::uint8_t>& b, std::size_t i) {
    return b.at(i) | (std::uint32_t(b.at(i+1)) << 8) | (std::uint32_t(b.at(i+2)) << 16);
}
struct Derived { audio::StreamConfig stream; bool rate_control; };
Derived derive(const usb::Configuration& c, usb::Speed speed, Selection s,
               std::uint32_t rate, bool capture) {
    const usb::Alternate* alt = nullptr;
    for (const auto& a : c.alternates)
        if (a.interface_number == s.interface_number && a.alternate_setting == s.alternate_setting) {
            if (alt) reject("duplicate selected alternate");
            alt = &a;
        }
    if (!alt || alt->interface_class != 1 || alt->interface_subclass != 2 || alt->interface_protocol != 0)
        reject("selected alternate must advertise UAC1 audio streaming");
    const usb::Endpoint* endpoint = nullptr;
    for (const auto& e : alt->endpoints) if (e.address == s.endpoint) endpoint = &e;
    if (!endpoint || endpoint->transfer_type() != 1 || endpoint->usage_type() != 0 || endpoint->interval != 1 ||
        (bool(endpoint->address & 0x80) != capture)) reject("selected data endpoint unsupported");
    const auto& raw_endpoint = c.descriptors.at(endpoint->descriptor_index).bytes;
    if (raw_endpoint.size() != 9 || raw_endpoint[7] || raw_endpoint[8])
        reject("unsupported UAC1 endpoint refresh/synchronization topology");
    const auto capacity = endpoint->bytes_per_service_interval(speed);
    const auto interval = endpoint->service_interval_ns(speed);
    if (!capacity || !interval || (*interval != 1000000 && *interval != 125000)) reject("unsupported service interval");
    bool general = false, format = false, rate_supported = false, rate_control = false, endpoint_general = false, fixed_rate = false;
    std::size_t channels = 0;
    PcmFormat pcm{};
    // Class-specific interface descriptors must precede endpoints. Their scope
    // ends at the next interface; endpoint-specific descriptors bind to their
    // immediately preceding standard endpoint, never to a neighbouring alternate.
    const usb::Endpoint* current_endpoint = nullptr;
    bool saw_endpoint = false;
    for (std::size_t i = alt->descriptor_index + 1; i < c.descriptors.size(); ++i) {
        const auto& b = c.descriptors[i].bytes;
        if (b.size() < 2) reject("short descriptor");
        if (b[1] == 4) break;
        if (b[1] == 5) {
            saw_endpoint = true;
            current_endpoint = nullptr;
            if (i == endpoint->descriptor_index) current_endpoint = endpoint;
        } else if (b[1] == 0x24) {
            if (saw_endpoint) reject("interface format descriptor follows endpoint");
            if (b.size() < 3) reject("short audio descriptor");
            if (b[2] == 1) {
                if (general || b.size() != 7 || b[5] != 1 || b[6] != 0) reject("unsupported PCM format tag");
                general = true;
            } else if (b[2] == 2) {
                if (format || b.size() < 8 || b[3] != 1 || b[4] == 0 ||
                    !((b[5] == 2 && b[6] == 16) || (b[5] == 3 && b[6] == 24))) reject("unsupported Type-I PCM packing");
                format = true; channels = b[4]; pcm = static_cast<PcmFormat>(b[5]);
                if (b[7] == 0) {
                    if (b.size() != 14 || le24(b,8) == 0 || le24(b,8) > le24(b,11)) reject("invalid continuous rate range");
                    rate_supported = rate >= le24(b,8) && rate <= le24(b,11);
                    fixed_rate = le24(b,8) == le24(b,11);
                } else {
                    if (b.size() != std::size_t(8 + 3*b[7])) reject("invalid discrete rate table");
                    fixed_rate = b[7] == 1;
                    for (std::size_t n = 0; n < b[7]; ++n) {
                        if (le24(b,8+3*n) == 0) reject("zero sample rate");
                        if (rate == le24(b,8+3*n)) rate_supported = true;
                    }
                }
            } else reject("unsupported class-specific audio descriptor");
        } else if (b[1] == 0x25 && current_endpoint) {
            if (endpoint_general || b.size() != 7 || b[2] != 1 || (b[3] & 0xfc)) reject("unsupported UAC1 endpoint descriptor");
            endpoint_general = true; rate_control = (b[3] & 1) != 0;
            // Pitch control/lock delay are not implemented by this setup policy.
            if ((b[3] & 2) || b[4] || b[5] || b[6]) reject("unsupported pitch or lock delay");
        }
    }
    if (!general || !format || !rate_supported || !endpoint_general) reject("missing advertised audio format/rate");
    if (!rate_control && !fixed_rate) reject("selectable rates without supported rate control");
    AudioPacketConfig audio{rate, static_cast<std::uint32_t>(1000000000 / *interval), channels, pcm, *capacity};
    AudioPacketBounds bounds{};
    if (validate_audio_packet_config(audio, bounds) != BufferStatus::ok) reject("endpoint cannot carry nominal audio");
    return {{*endpoint, audio}, rate_control};
}
}

Plan make_plan(const usb::Configuration& c, usb::Speed speed, Selection capture,
               Selection playback, std::uint32_t rate) {
    if (!c.value || (speed != usb::Speed::full && speed != usb::Speed::high) ||
        (rate != 44100 && rate != 48000)) reject("unsupported configuration/speed/rate");
    if (capture.interface_number == playback.interface_number && capture.alternate_setting != playback.alternate_setting)
        reject("directions require conflicting alternates on one interface");
    const auto in = derive(c,speed,capture,rate,true);
    const auto out = derive(c,speed,playback,rate,false);
    Plan p;
    p.configuration = c.value;
    p.audio = {speed, in.stream, out.stream, 2, 8};
    p.interfaces[p.interface_count++] = {capture.interface_number,capture.alternate_setting};
    if (playback.interface_number != capture.interface_number)
        p.interfaces[p.interface_count++] = {playback.interface_number,playback.alternate_setting};
    if (in.rate_control) p.rates[p.rate_count++] = {capture.interface_number,capture.endpoint};
    if (out.rate_control) p.rates[p.rate_count++] = {playback.interface_number,playback.endpoint};
    return p;
}

Session::Session(Plan plan, Backend& backend) : plan_(std::move(plan)), backend_(backend) {}
Session::~Session() {
    if (state_ == State::draining || io_started_) std::terminate();
    if (opened_) rollback();
}
bool Session::fail(Error e) noexcept {
    error_ = e;
    if (io_started_) {
        state_ = State::draining;
        backend_.stop_io();
        poll();
    } else rollback();
    return false;
}
bool Session::prepare() noexcept {
    // An uncertain hardware state cannot be retried without new discovery and
    // an explicit new owner. Successful stop may reuse this validated plan.
    if (state_ != State::idle && state_ != State::failed) return false;
    state_ = State::preparing; error_ = Error::none; rollback_failed_ = false;
    std::uint8_t configuration = 0;
    if (!backend_.current_configuration(configuration) || configuration != plan_.configuration) return fail(Error::configuration);
    // Snapshot every original alternate before making any mutation.
    for (std::size_t i=0; i<plan_.interface_count; ++i) {
        const auto number = plan_.interfaces[i].number;
        if (!backend_.open_interface(number)) return fail(Error::open);
        ++opened_;
        if (!backend_.current_alternate(number,original_alternate_[i])) return fail(Error::alternate_read);
    }
    for (std::size_t i=0; i<plan_.interface_count; ++i) {
        ++alternates_changed_; // Failure may still have changed the setting.
        if (original_alternate_[i] != plan_.interfaces[i].alternate &&
            !backend_.select_alternate(plan_.interfaces[i].number,plan_.interfaces[i].alternate)) return fail(Error::alternate_select);
    }
    // Read ALL restorable rates before the first SET_CUR. Shared-clock endpoints
    // may alias; sequential read/write would otherwise lose the original rate.
    for (std::size_t i=0; i<plan_.rate_count; ++i)
        if (!backend_.read_rate(plan_.rates[i],original_rate_[i]) || !original_rate_[i] || original_rate_[i] > 0xffffff)
            return fail(Error::rate_read);
    for (std::size_t i=0; i<plan_.rate_count; ++i) {
        ++rates_changed_;
        if (original_rate_[i] != plan_.audio.capture.audio.sample_rate &&
            !backend_.write_rate(plan_.rates[i],plan_.audio.capture.audio.sample_rate)) return fail(Error::rate_write);
    }
    state_ = State::ready;
    return true;
}
bool Session::start(std::uint64_t first_service) noexcept {
    if (state_ != State::ready || !first_service ||
        (plan_.audio.speed == usb::Speed::high && (first_service % 8))) return false;
    io_started_ = true;
    if (!backend_.start_io(plan_.audio,first_service)) return fail(Error::start);
    state_ = State::running;
    return true;
}
void Session::stop() noexcept {
    if (state_ == State::running) {
        state_ = State::draining; backend_.stop_io(); poll();
    } else if (state_ == State::ready) rollback();
}
void Session::poll() noexcept {
    if (state_ == State::draining && backend_.io_drained()) {
        io_started_ = false;
        rollback();
    }
}
void Session::rollback() noexcept {
    while (rates_changed_) {
        const auto i = --rates_changed_;
        if (!backend_.write_rate(plan_.rates[i],original_rate_[i])) rollback_failed_ = true;
    }
    while (alternates_changed_) {
        const auto i = --alternates_changed_;
        if (!backend_.select_alternate(plan_.interfaces[i].number,original_alternate_[i])) rollback_failed_ = true;
    }
    while (opened_) backend_.close_interface(plan_.interfaces[--opened_].number);
    if (rollback_failed_) {
        if (error_ == Error::none) error_ = Error::rollback;
        state_ = State::uncertain;
    } else state_ = error_ == Error::none ? State::idle : State::failed;
}
} // namespace virus::session
