#include "virus/iousbhost_backend.hpp"
#import <IOUSBHost/IOUSBHost.h>
#include <IOKit/usb/USB.h>
#include <mach/mach_time.h>
#include <cstring>
#include <stdexcept>

namespace virus::audio {
namespace {
constexpr std::size_t backing_bytes = max_packets * 3072;
PacketStatus translate(IOReturn status) noexcept {
    switch (status) {
        // USB data underrun means a short transaction. Engine validates the byte
        // count/alignment and records partial playback; variable-size IN is valid.
        case kIOReturnSuccess: case kIOReturnUnderrun: return PacketStatus::success;
        case kIOReturnAborted: return PacketStatus::cancelled;
        case kIOReturnNoDevice: case kIOReturnNotResponding: return PacketStatus::disconnected;
        case kIOUSBPipeStalled: return PacketStatus::stalled;
        case kIOReturnIsoTooOld: case kIOReturnIsoTooNew:
        case kIOUSBNotSent1Err: case kIOUSBNotSent2Err: return PacketStatus::missed;
        default: return PacketStatus::error;
    }
}
bool matches(IOUSBHostPipe* pipe, const usb::Endpoint& expected) {
    if (!pipe || !pipe.descriptors) return false;
    const auto& d = pipe.descriptors->descriptor;
    return d.bEndpointAddress == expected.address && d.bmAttributes == expected.attributes &&
        d.bInterval == expected.interval && OSSwapLittleToHostInt16(d.wMaxPacketSize) == expected.max_packet_size_raw;
}
}
struct IOUSBHostBackend::Impl {
    struct Slot {
        NSMutableData* __strong data = nil;
        IOUSBHostIsochronousTransactionCompletionHandler __strong handler = nil;
        std::array<IOUSBHostIsochronousTransaction,max_packets> transactions{};
        const Request* request = nullptr;
        Token token{};
    };
    Config config;
    IOUSBHostPipe* __strong capture;
    IOUSBHostPipe* __strong playback;
    Engine* engine = nullptr;
    std::array<Slot,max_requests> slots;
    std::array<bool,2> abort_pending{};
    mach_timebase_info_data_t timebase{};
    IOReturn error = kIOReturnSuccess;
    Impl(Config c, IOUSBHostPipe* in, IOUSBHostPipe* out) : config(c),capture(in),playback(out) {
        if ((c.speed != usb::Speed::full && c.speed != usb::Speed::high) ||
            (c.speed == usb::Speed::high && c.packets_per_request != 8) ||
            !matches(in,c.capture.endpoint) || !matches(out,c.playback.endpoint) ||
            c.capture.endpoint.interval != 1 || c.playback.endpoint.interval != 1 ||
            c.capture.endpoint.transfer_type() != 1 || c.playback.endpoint.transfer_type() != 1 ||
            !c.requests_per_direction || c.requests_per_direction > max_requests / 2 ||
            !c.packets_per_request || c.packets_per_request > max_packets)
            throw std::invalid_argument("IOUSBHost adapter requires matching USB2 isochronous data pipes");
        if (mach_timebase_info(&timebase) != KERN_SUCCESS || !timebase.denom)
            throw std::runtime_error("mach timebase unavailable");
        for (std::size_t i = 0; i < slots.size(); ++i) {
            slots[i].data = [[NSMutableData alloc] initWithLength:backing_bytes];
            if (!slots[i].data) throw std::bad_alloc();
            slots[i].handler = ^(IOReturn status, IOUSBHostIsochronousTransaction* transactions) {
                finish(i,status,transactions);
            };
        }
    }
    ~Impl() { for (const auto& s : slots) if (s.request) std::terminate(); }
    void finish(std::size_t index, IOReturn overall, IOUSBHostIsochronousTransaction* transactions) noexcept {
        auto& s = slots[index];
        if (!s.request || !engine) std::terminate(); // Violated terminal callback/lifetime contract.
        const auto& r = *s.request;
        Completion c{}; c.token = s.token; c.packet_count = r.packet_count;
        const bool input = r.direction == Direction::capture;
        for (std::size_t p = 0; p < r.packet_count; ++p) {
            auto& result = c.packets[p];
            const auto& t = transactions[p];
            auto status = t.status == kIOReturnInvalid ?
                (overall == kIOReturnSuccess ? kIOReturnError : overall) : t.status;
            if (overall == kIOReturnNoDevice || overall == kIOReturnNotResponding) status = overall;
            result.status = translate(status);
            result.actual_bytes = t.completeCount;
            const __uint128_t ns = static_cast<__uint128_t>(t.timeStamp) * timebase.numer / timebase.denom;
            result.timestamp_valid = t.timeStamp && ns <= UINT64_MAX;
            result.host_time_ns = result.timestamp_valid ? static_cast<std::uint64_t>(ns) : 0;
            const auto& packet = r.packets[p];
            if (input && result.status == PacketStatus::success && result.actual_bytes <= packet.requested_bytes)
                std::memcpy(r.bytes + packet.offset,
                            static_cast<const std::uint8_t*>(s.data.bytes) + packet.offset,result.actual_bytes);
        }
        // Framework has released this I/O. Release adapter ownership before the
        // engine may stop/cancel peers, or pump and reuse this slot after callback.
        s.request = nullptr;
        engine->complete(c);
        engine->pump();
    }
};
IOUSBHostBackend::IOUSBHostBackend(const Config& c, IOUSBHostPipe* in, IOUSBHostPipe* out)
    : impl_(std::make_unique<Impl>(c,in,out)) {}
IOUSBHostBackend::~IOUSBHostBackend() = default;
void IOUSBHostBackend::bind(Engine& e) noexcept {
    if (impl_->engine || e.state() != State::idle) std::terminate();
    impl_->engine = &e;
}
std::int32_t IOUSBHostBackend::last_error() const noexcept { return impl_->error; }
SubmitStatus IOUSBHostBackend::submit(const Request& r) noexcept {
    auto& i = *impl_;
    const bool input = r.direction == Direction::capture;
    const auto d = input ? 0u : 1u;
    const auto& stream = input ? i.config.capture : i.config.playback;
    if (!i.engine || r.token.slot >= i.slots.size() || !r.bytes || !r.first_service ||
        r.endpoint != stream.endpoint.address || r.capacity > backing_bytes ||
        r.packet_count != i.config.packets_per_request ||
        r.interval_ns != (i.config.speed == usb::Speed::high ? 125000u : 1000000u) ||
        (i.config.speed == usb::Speed::high && r.first_service % 8)) {
        i.error = kIOReturnBadArgument; return SubmitStatus::rejected;
    }
    auto& s = i.slots[r.token.slot];
    if (s.request) { i.error = kIOReturnBusy; return SubmitStatus::rejected; }
    for (std::size_t p = 0; p < r.packet_count; ++p) {
        const auto& packet = r.packets[p];
        if (packet.offset > r.capacity || packet.requested_bytes > r.capacity - packet.offset ||
            packet.requested_bytes > stream.audio.max_packet_bytes) {
            i.error = kIOReturnBadArgument; return SubmitStatus::rejected;
        }
        s.transactions[p] = {kIOReturnInvalid,static_cast<std::uint32_t>(packet.requested_bytes),
            static_cast<std::uint32_t>(packet.offset),0,0,IOUSBHostIsochronousTransactionOptionsNone};
        if (!input) std::memcpy(static_cast<std::uint8_t*>(s.data.mutableBytes) + packet.offset,
                               r.bytes + packet.offset,packet.requested_bytes);
    }
    bool peers = false;
    for (const auto& peer : i.slots)
        if (peer.request && peer.request->direction == r.direction) peers = true;
    if (!peers) i.abort_pending[d] = false;
    if (i.abort_pending[d]) { i.error = kIOReturnAborted; return SubmitStatus::rejected; }
    s.request = &r; s.token = r.token;
    NSError* error = nil;
    IOUSBHostPipe* pipe = input ? i.capture : i.playback;
    const auto first_frame = r.first_service / (i.config.speed == usb::Speed::high ? 8 : 1);
    const BOOL accepted = [pipe enqueueIORequestWithData:s.data transactionList:s.transactions.data()
        transactionListCount:r.packet_count firstFrameNumber:first_frame options:IOUSBHostIsochronousTransferOptionsNone
        error:&error completionHandler:s.handler];
    if (!accepted) {
        s.request = nullptr;
        i.error = error ? static_cast<IOReturn>(error.code) : kIOReturnError;
        return SubmitStatus::rejected;
    }
    return SubmitStatus::accepted;
}
void IOUSBHostBackend::cancel(Token token) noexcept {
    auto& i = *impl_;
    if (token.slot >= i.slots.size()) return;
    auto& s = i.slots[token.slot];
    if (!s.request || !(s.token == token)) return;
    const auto d = s.request->direction == Direction::capture ? 0u : 1u;
    if (i.abort_pending[d]) return;
    IOUSBHostPipe* pipe = d == 0 ? i.capture : i.playback;
    NSError* error = nil;
    if ([pipe abortWithOption:IOUSBHostAbortOptionAsynchronous error:&error]) i.abort_pending[d] = true;
    else i.error = error ? static_cast<IOReturn>(error.code) : kIOReturnError;
    // Never synthesize completions or free request storage on abort failure.
}
} // namespace virus::audio
