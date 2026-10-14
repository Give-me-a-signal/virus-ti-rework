#include "midi_core.hpp"
#include "legacy_ring_snapshot.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <vector>

using namespace virus::midi;
struct Captured {
    std::vector<std::uint8_t> bytes;
    Error error;
    std::uint64_t timestamp;
    bool operator==(const Captured& other) const {
        return bytes == other.bytes && error == other.error && timestamp == other.timestamp;
    }
};
using Log = std::vector<Captured>;
void sink(void* context, const Event& event) {
    auto& log = *static_cast<Log*>(context);
    std::vector<std::uint8_t> bytes;
    if (event.size) bytes.assign(event.bytes, event.bytes + event.size);
    log.push_back({bytes, event.error, event.timestamp});
}
void message(const Captured& c, std::initializer_list<std::uint8_t> bytes,
             std::uint64_t timestamp = 7) {
    assert(c.error == Error::none && c.bytes == std::vector<std::uint8_t>(bytes));
    assert(c.timestamp == timestamp);
}
int main() {
    {
        std::array<std::uint8_t,4096> snapshot{};
        const auto put = [&](std::size_t offset, std::uint32_t word) {
            for(unsigned i=0;i<4;++i) snapshot[offset+i]=static_cast<std::uint8_t>(word>>(i*8));
        };
        put(8,1021);
        LegacyRingInfo info{};
        assert(copy_legacy_ring_snapshot(snapshot.data(),snapshot.size(),nullptr,0,info)==SnapshotResult::ok);
        assert(info.queued_words==0);
        put(0,1020); put(4,2);
        put(12+1020*4,0x12345678); put(12,0xfedcba98); put(16,0x80000000);
        std::array<std::uint32_t,1020> words{};
        assert(copy_legacy_ring_snapshot(snapshot.data(),snapshot.size(),words.data(),words.size(),info)==SnapshotResult::ok);
        assert(info.queued_words==3 && words[0]==0x12345678 && words[1]==0xfedcba98 && words[2]==0x80000000);
        words[0]=42;
        assert(copy_legacy_ring_snapshot(snapshot.data(),snapshot.size(),words.data(),2,info)==SnapshotResult::output_too_small);
        assert(words[0]==42);
        // Full is one slot short of capacity, including wrap.
        put(0,1); put(4,0);
        assert(copy_legacy_ring_snapshot(snapshot.data(),snapshot.size(),words.data(),words.size(),info)==SnapshotResult::ok);
        assert(info.queued_words==1020);
        put(0,1021);
        assert(inspect_legacy_ring_snapshot(snapshot.data(),snapshot.size(),info)==SnapshotResult::invalid_header);
        put(0,0);put(8,0xffffffff);
        assert(inspect_legacy_ring_snapshot(snapshot.data(),snapshot.size(),info)==SnapshotResult::invalid_header);
        assert(inspect_legacy_ring_snapshot(snapshot.data(),4095,info)==SnapshotResult::invalid_size);
        assert(inspect_legacy_ring_snapshot(nullptr,4096,info)==SnapshotResult::invalid_size);
    }
    {
        Log log; StreamParser<> p(sink, &log);
        const std::uint8_t bytes[]{0xb0, 40, 0xf8, 64, 41, 65, 0xcf, 23, 24, 0xf2, 1, 2};
        p.feed(bytes, sizeof bytes, 7); p.finish();
        assert(log.size() == 6);
        message(log[0], {0xf8}); message(log[1], {0xb0,40,64});
        message(log[2], {0xb0,41,65}); message(log[3], {0xcf,23});
        message(log[4], {0xcf,24}); message(log[5], {0xf2,1,2});
    }
    {
        Log log; StreamParser<5> p(sink, &log);
        p.push(0xf0, 2); p.push(0, 3); p.push(0xf8, 4); p.push(32, 5);
        p.push(51, 6); p.push(0xf7, 8);
        assert(log.size() == 2);
        message(log[0], {0xf8}, 4); message(log[1], {0xf0,0,32,51,0xf7}, 2);
        p.push(0x90,9); p.push(60,10); p.push(1,11); p.push(61,12); p.push(2,13);
        message(log[2], {0x90,60,1},9); message(log[3], {0x90,61,2},12);
    }
    {
        Log log; StreamParser<4> p(sink, &log);
        const std::uint8_t bytes[]{0xf0,1,2,3,4,5,0xf8,6,0xf7,0x90,60,100};
        p.feed(bytes, sizeof bytes, 7); p.finish();
        assert(log.size() == 3 && log[0].error == Error::sysex_overflow);
        message(log[1], {0xf8}); message(log[2], {0x90,60,100});
        // F7 itself must fit; a full buffer without terminator is not a message.
        log.clear(); const std::uint8_t exact[]{0xf0,1,2,3,0xf7};
        p.feed(exact, sizeof exact, 7); p.finish();
        assert(log.size() == 1 && log[0].error == Error::sysex_overflow);
    }
    {
        Log log; StreamParser<> p(sink, &log);
        const std::uint8_t bytes[]{1,0xf7,0x90,60,0xb0,40,64,0xf6,65,0xf4,0xf5,0xf9,0xfd};
        p.feed(bytes, sizeof bytes, 7); p.finish();
        assert(log.size() == 10);
        assert(log[0].error == Error::stray_data);
        assert(log[1].error == Error::unexpected_end);
        assert(log[2].error == Error::interrupted);
        message(log[3], {0xb0,40,64}); message(log[4], {0xf6});
        assert(log[5].error == Error::stray_data);
        for (unsigned i=6;i<10;++i) assert(log[i].error == Error::undefined_status);
    }
    {
        Log log; StreamParser<> p(sink, &log);
        const std::uint8_t bytes[]{0xf0,1,0xf0,2,0xf7,0xf1,4,0xf3,5,0xd0,6};
        p.feed(bytes, sizeof bytes, 7); p.finish();
        assert(log.size()==5 && log[0].error==Error::interrupted);
        message(log[1],{0xf0,2,0xf7}); message(log[2],{0xf1,4});
        message(log[3],{0xf3,5}); message(log[4],{0xd0,6});
        log.clear(); p.push(0xf0,7); p.finish(7);
        assert(log.size()==1 && log[0].error==Error::truncated);
        log.clear(); p.push(0x90,7); p.push(60,7); p.finish(7);
        assert(log.size()==1 && log[0].error==Error::truncated);
        log.clear(); p.feed(nullptr,0,7); p.feed(nullptr,1,7);
        assert(log.size()==1 && log[0].error==Error::invalid_buffer);
    }
    {
        // Byte-identical offline fixtures; no hardware behavior assumed.
        std::array<std::uint8_t,3> out{1,2,3};
        assert(build_control_change(0,40,127,out));
        assert((out==std::array<std::uint8_t,3>{0xb0,40,127}));
        assert(build_poly_pressure(15,15,7,out));
        assert((out==std::array<std::uint8_t,3>{0xaf,15,7}));
        const auto before=out;
        assert(!build_control_change(16,40,127,out) && out==before);
        assert(!build_control_change(0,128,127,out) && out==before);
        assert(!build_poly_pressure(0,15,128,out) && out==before);
        std::array<std::uint8_t,11> bank{};
        assert(build_declared_bank_write(Bank::C,3,5,123,1,bank));
        assert((bank==std::array<std::uint8_t,11>{0xf0,0,0x20,0x33,1,3,0x6e,5,123,1,0xf7}));
        const auto original=bank;
        assert(!build_declared_bank_write(static_cast<Bank>(117),3,5,123,1,bank));
        assert(!build_declared_bank_write(Bank::C,128,5,123,1,bank));
        assert(!build_declared_bank_write(Bank::C,3,128,123,1,bank));
        assert(!build_declared_bank_write(Bank::C,3,5,128,1,bank));
        assert(!build_declared_bank_write(Bank::C,3,5,123,128,bank));
        assert(bank==original);
        for(unsigned command=110;command<=116;++command) {
            assert(build_declared_bank_write(static_cast<Bank>(command),127,127,127,127,bank));
            Log log; StreamParser<> p(sink,&log); p.feed(bank.data(),bank.size(),7);p.finish();
            assert(log.size()==1 && log[0].error==Error::none);
            assert(std::equal(bank.begin(),bank.end(),log[0].bytes.begin()));
        }
    }
    {
        // Deterministic malformed-byte stress and arbitrary buffer boundaries.
        std::mt19937 rng(0x71517);
        for(unsigned round=0;round<1000;++round) {
            std::vector<std::uint8_t> bytes(1024);
            for(auto& b:bytes) b=static_cast<std::uint8_t>(rng());
            Log whole,chunked; StreamParser<32> a(sink,&whole),b(sink,&chunked);
            a.feed(bytes.data(),bytes.size(),7);a.finish(7);
            std::size_t i=0;
            while(i<bytes.size()) {
                const auto count=std::min<std::size_t>(1+rng()%31,bytes.size()-i);
                b.feed(bytes.data()+i,count,7);i+=count;
            }
            b.finish(7); assert(whole==chunked);
        }
    }
    std::cout << "MIDI core: byte fixtures, error recovery, bounds, 1000 chunking stress cases passed\n";
}
