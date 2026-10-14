#include "virus/audio_engine.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>

static bool forbid_allocations = false;
void* operator new(std::size_t n) {
    if (forbid_allocations) std::abort();
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #x); std::abort(); } } while (0)
using namespace virus;
using namespace virus::audio;

struct FakeBackend final : Backend {
    struct Pending { const Request* request = nullptr; Token token{}; bool cancelled = false; };
    std::array<Pending,max_requests> pending{};
    std::size_t calls = 0, reject_on_call = 0, cancellations = 0;
    SubmitStatus submit(const Request& r) noexcept override {
        ++calls;
        if (calls == reject_on_call) return SubmitStatus::rejected;
        CHECK(!pending[r.token.slot].request);
        CHECK(r.capacity >= r.packet_count * r.packets[0].requested_bytes);
        pending[r.token.slot] = {&r,r.token,false};
        return SubmitStatus::accepted;
    }
    void cancel(Token token) noexcept override {
        auto& p = pending[token.slot];
        CHECK(p.request && p.token == token && !p.cancelled);
        p.cancelled = true; ++cancellations;
    }
    const Request& request(std::size_t slot) { CHECK(pending[slot].request); return *pending[slot].request; }
    Completion result(std::size_t slot, PacketStatus status = PacketStatus::success,
                      std::size_t capture_bytes = 192, std::uint8_t value = 1) {
        const auto& r = request(slot);
        Completion c{}; c.token = r.token; c.packet_count = r.packet_count;
        for (std::size_t i = 0; i < c.packet_count; ++i) {
            const auto& p = r.packets[i];
            const auto count = r.direction == Direction::capture ? capture_bytes : p.requested_bytes;
            c.packets[i] = {status,count,true,1000000 + r.first_service + i};
            if (r.direction == Direction::capture && count <= p.requested_bytes) {
                std::fill_n(r.bytes + p.offset, p.requested_bytes, 0xee);
                for (std::size_t b = 0; b < count; ++b) r.bytes[p.offset + b] = b % 2 ? 0 : value;
            }
        }
        return c;
    }
    void deliver(Engine& engine, Completion c) {
        CHECK(pending[c.token.slot].request);
        pending[c.token.slot] = {};
        engine.complete(c);
    }
    void drain(Engine& engine) {
        for (std::size_t s = 0; s < max_requests; ++s)
            if (pending[s].request) deliver(engine, result(s,PacketStatus::cancelled,0));
        CHECK(engine.state() == State::idle && engine.outstanding() == 0);
    }
};
Config config(std::size_t packets = 2, std::size_t requests = 2,
              usb::Speed speed = usb::Speed::full, PcmFormat format = PcmFormat::signed_16_le) {
    const auto intervals = speed == usb::Speed::high ? 8000u : 1000u;
    const auto packet_bytes = format == PcmFormat::signed_16_le ? 256u : 384u;
    return {speed,
        {{0,0x81,1,1,static_cast<std::uint16_t>(packet_bytes)}, {48000,intervals,2,format,packet_bytes}},
        {{1,0x02,1,1,static_cast<std::uint16_t>(packet_bytes)}, {48000,intervals,2,format,packet_bytes}},
        requests,packets};
}

void lifecycle_and_order() {
    FakeBackend b;
    AudioFrameQueue capture(512,2), playback(512,2);
    Engine e(config(),b,capture,playback);
    std::array<std::int32_t,192> samples{};
    samples.fill(0x12340000);
    CHECK(playback.push(samples.data(),96,samples.size()) == QueueStatus::ok);
    forbid_allocations = true;
    CHECK(e.start(200)); CHECK(!e.start(300));
    CHECK(e.outstanding() == 4 && b.calls == 4 && e.stats().underrun_packets == 2);
    CHECK(b.request(2).bytes[0] == 0x34 && b.request(2).bytes[1] == 0x12);
    CHECK(b.request(3).bytes[0] == 0 && b.request(3).bytes[1] == 0);
    const auto old = b.result(1,PacketStatus::success,192,2);
    b.deliver(e,old); // Later capture completion cannot overtake earlier audio.
    CHECK(e.outstanding() == 4 && e.stats().capture_frames == 0);
    e.pump(); CHECK(b.calls == 4); // Completed but not retired slot must not be reused.
    e.complete(old); CHECK(e.stats().stale_completions == 1);
    b.deliver(e,b.result(0,PacketStatus::success,192,1));
    CHECK(e.stats().capture_frames == 192 && e.outstanding() == 2);
    CHECK(e.stats().last_capture_service == 203 && e.stats().last_capture_host_ns == 1000203);
    CHECK(capture.pop(samples.data(),96,samples.size()) == QueueStatus::ok);
    for (auto s : samples) CHECK(s == 0x00010000);
    CHECK(capture.pop(samples.data(),96,samples.size()) == QueueStatus::ok);
    for (auto s : samples) CHECK(s == 0x00020000);
    e.pump(); CHECK(b.calls == 6);
    // Late callback from the same generation must not release newly reused slot.
    e.complete(old); CHECK(e.stats().stale_completions == 2 && e.outstanding() == 4);
    const auto old_generation = b.result(0);
    const auto* owned_bytes = b.request(2).bytes;
    e.stop(); e.stop();
    CHECK(e.state() == State::draining && e.outstanding() == 4 && b.cancellations == 4);
    CHECK(!e.start(400)); CHECK(owned_bytes[0] == 0x34);
    e.pump(); CHECK(b.calls == 6);
    b.drain(e);
    CHECK(e.start(1000));
    e.complete(old_generation); CHECK(e.stats().stale_completions == 3 && e.outstanding() == 4);
    e.stop(); b.drain(e);
    forbid_allocations = false;
}
void failure_and_drain() {
    for (std::size_t fail = 1; fail <= 4; ++fail) {
        FakeBackend b; b.reject_on_call = fail;
        AudioFrameQueue in(512,2),out(512,2);
        Engine e(config(),b,in,out);
        forbid_allocations = true;
        CHECK(!e.start(1) && e.failed());
        CHECK(e.outstanding() == fail - 1 && b.cancellations == fail - 1);
        b.drain(e);
        CHECK(e.stats().submission_errors == 1);
        CHECK(e.start(50)); e.stop(); b.drain(e);
        forbid_allocations = false;
    }
    for (unsigned bad = 0; bad != 5; ++bad) {
        FakeBackend b; AudioFrameQueue in(512,2),out(512,2);
        Engine e(config(),b,in,out);
        CHECK(e.start(1));
        auto c = b.result(0);
        if (bad == 0) ++c.packet_count;
        if (bad == 1) c.packets[0].actual_bytes = 257;
        if (bad == 2) c.packets[0].actual_bytes = 3;
        if (bad == 3) c.packets[0].status = PacketStatus::disconnected;
        if (bad == 4) c.packets[0].status = PacketStatus::stalled;
        forbid_allocations = true;
        b.deliver(e,c);
        CHECK(e.failed() && e.state() == State::draining && b.cancellations == 3);
        b.drain(e);
        forbid_allocations = false;
    }
}
void packets_and_overflow() {
    FakeBackend b; AudioFrameQueue in(64,2),out(64,2);
    Engine e(config(2,1),b,in,out);
    CHECK(e.start(0));
    auto c = b.result(0,PacketStatus::success,192,9);
    b.deliver(e,c); // First packet fits, second explicitly drops on overrun.
    CHECK(e.stats().capture_frames == 48 && e.stats().overrun_packets == 1);
    e.pump();
    c = b.result(0,PacketStatus::success,64);
    c.packets[0].status = PacketStatus::error; // Partial error payload is discarded.
    c.packets[1].actual_bytes = 0;
    b.deliver(e,c);
    CHECK(e.stats().packet_errors == 1 && e.stats().capture_frames == 48);
    c = b.result(1); c.packets[0].actual_bytes = 4;
    c.packets[1].status = PacketStatus::missed;
    b.deliver(e,c);
    CHECK(e.stats().partial_packets == 1 && e.stats().playback_frames == 1);
    CHECK(e.stats().discontinuities == 5 && !e.stats().capture_timestamp_valid);
    e.stop(); b.drain(e);
}
void all_completion_orders() {
    std::array<unsigned,4> order{0,1,2,3};
    std::size_t permutations = 0;
    do {
        FakeBackend b; AudioFrameQueue in(1024,2),out(1024,2);
        Engine e(config(1),b,in,out);
        forbid_allocations = true;
        for (unsigned cycle = 0; cycle != 100; ++cycle) {
            if (!cycle) CHECK(e.start(0)); else e.pump();
            for (auto slot : order) b.deliver(e,b.result(slot,PacketStatus::success,192,slot + 1));
            CHECK(e.outstanding() == 0);
            std::array<std::int32_t,192> samples{};
            CHECK(in.pop(samples.data(),96,samples.size()) == QueueStatus::ok);
            for (std::size_t n = 0; n < samples.size(); ++n) CHECK(samples[n] == (n < 96 ? 0x10000 : 0x20000));
        }
        e.stop(); CHECK(e.state() == State::idle);
        forbid_allocations = false;
        ++permutations;
    } while (std::next_permutation(order.begin(),order.end()));
    CHECK(permutations == 24);
}
void stop_completion_interleavings() {
    // Race semantics are serialized: stop may be before/after any terminal callback.
    for (unsigned stop_after = 0; stop_after <= 4; ++stop_after) {
        std::array<unsigned,4> order{0,1,2,3};
        do {
            FakeBackend b; AudioFrameQueue in(512,2),out(512,2);
            Engine e(config(1),b,in,out); CHECK(e.start(0));
            for (unsigned i = 0; i <= 4; ++i) {
                if (i == stop_after) e.stop();
                if (i < 4) b.deliver(e,b.result(order[i]));
            }
            CHECK(e.state() == State::idle && !e.outstanding());
        } while (std::next_permutation(order.begin(),order.end()));
    }
}
void format24_and_high_speed() {
    FakeBackend b; AudioFrameQueue in(64,2),out(64,2);
    Engine e(config(1,1,usb::Speed::high,PcmFormat::signed_24_le),b,in,out);
    std::array<std::int32_t,12> samples{}; samples.fill(0x12345600);
    CHECK(out.push(samples.data(),6,samples.size()) == QueueStatus::ok);
    CHECK(e.start(8000));
    CHECK(b.request(1).packets[0].requested_bytes == 36 && b.request(1).interval_ns == 125000);
    const auto& r = b.request(1);
    CHECK(r.bytes[0] == 0x56 && r.bytes[1] == 0x34 && r.bytes[2] == 0x12);
    auto c = b.result(0,PacketStatus::success,36);
    auto& capture = b.request(0);
    CHECK(pcm_encode(PcmFormat::signed_24_le,samples.data(),12,2,capture.bytes,36) == BufferStatus::ok);
    b.deliver(e,c);
    samples.fill(0);
    CHECK(in.pop(samples.data(),6,samples.size()) == QueueStatus::ok);
    for (auto s : samples) CHECK(s == 0x12345600);
    e.stop(); b.drain(e);
}
void validation_and_overflow() {
    FakeBackend b; AudioFrameQueue in(512,2),out(512,2);
    for (unsigned bad = 0; bad < 8; ++bad) {
        auto c = config();
        if (bad == 0) c.capture.endpoint.address = 1;
        if (bad == 1) c.capture.endpoint.attributes = 2;
        if (bad == 2) c.capture.endpoint.interval = 2;
        if (bad == 3) c.requests_per_direction = 5;
        if (bad == 4) c.packets_per_request = 9;
        if (bad == 5) c.capture.audio.max_packet_bytes = 255;
        if (bad == 6) c.speed = usb::Speed::super;
        if (bad == 7) c.capture.audio.sample_rate = 44100;
        bool threw = false;
        try { Engine e(c,b,in,out); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw && !b.calls);
    }
    Engine e(config(),b,in,out);
    CHECK(!e.start(UINT64_MAX) && e.failed() && !b.calls && e.state() == State::idle);
    Completion stale{}; stale.token.slot = max_requests;
    e.complete(stale); CHECK(e.stats().stale_completions == 1);
}
void observer_vectors() {
    RecoveredFrameObserver fs(44100,usb::Speed::full);
    const std::array<unsigned,16> expected{45,44,44,44,44,44,44,44,44,44,45,44,44,44,44,44};
    for (auto n : expected) CHECK(fs.next_output_frames() == n);
    CHECK(!fs.synchronized());
    for (unsigned i = 0; i < 16; ++i) CHECK(fs.observe(44) == 44);
    CHECK(fs.synchronized());
    CHECK(fs.observe(47) == 45 && !fs.synchronized() && fs.errors_since_sync() == 1);
    for (unsigned i = 0; i < 15; ++i) fs.observe(44);
    CHECK(fs.synchronized() && !fs.errors_since_sync());
    CHECK(fs.observe(42) == 42 && fs.observe(46) == 46);
    CHECK(fs.next_output_frames() == 42 && fs.next_output_frames() == 46);
    RecoveredFrameObserver hs(44100,usb::Speed::high);
    const std::array<unsigned,16> prefix{6,6,5,6,5,6,5,6,5,6,5,6,5,6,5,6};
    std::uint32_t sum = 0;
    for (auto n : prefix) { CHECK(hs.next_output_frames() == n); sum += n; }
    for (unsigned i = 16; i < 128; ++i) {
        const auto n = hs.next_output_frames();
        if (i == 79 || i == 80 || i == 81) CHECK(n == 6);
        sum += n;
    }
    CHECK(sum == 706 && hs.observe(7) == 6 && hs.errors_since_sync() == 1);
    CHECK(hs.observe(4) == 4);
    for (auto speed : {usb::Speed::full,usb::Speed::high}) {
        RecoveredFrameObserver o(48000,speed);
        for (std::size_t i = 0; i < o.length(); ++i)
            CHECK(o.next_output_frames() == (speed == usb::Speed::full ? 48u : 6u));
    }
}
int main() {
    lifecycle_and_order(); failure_and_drain(); packets_and_overflow();
    all_completion_orders(); stop_completion_interleavings(); format24_and_high_speed();
    validation_and_overflow(); observer_vectors();
    std::puts("audio-engine: lifecycle, ownership, 2400 callback permutations, 120 stop interleavings, PCM/status/queue/timestamp validation and recovered observer vectors passed");
}
