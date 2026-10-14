#include "virus/usb_session.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
using namespace virus;
using namespace virus::session;
namespace {
std::vector<std::uint8_t> fixture(bool variable=true) {
    std::vector<std::uint8_t> b{9,2,0,0,2,1,0,0x80,50};
    for (unsigned n=1;n<=2;++n) {
        std::vector<std::uint8_t> d{
            9,4,static_cast<std::uint8_t>(n),0,0,1,2,0,0,
            9,4,static_cast<std::uint8_t>(n),1,1,1,2,0,0,
            7,0x24,1,1,0,1,0,
            11,0x24,2,1,2,3,24,1,0x80,0xbb,0,
            9,5,static_cast<std::uint8_t>(n==1 ? 0x81:2),1,0x26,1,1,0,0,
            7,0x25,1,static_cast<std::uint8_t>(variable),0,0,0};
        b.insert(b.end(),d.begin(),d.end());
    }
    b[2]=b.size() & 255;b[3]=b.size()>>8;
    return b;
}
Plan plan(usb::Speed speed=usb::Speed::full) {
    return make_plan(usb::parse_configuration(fixture()),speed,{1,1,0x81},{2,1,2},48000);
}
struct Fake : Backend {
    std::vector<std::string> log;
    int calls=0, fail_at=-1;
    bool rollback_failure=false, drained=true, partial_start=false;
    std::uint8_t configuration=1;
    std::array<bool,3> opened{};
    std::array<std::uint8_t,3> alt{};
    std::array<std::uint32_t,3> rate{0,44100,44100};
    bool record(std::string s) { log.push_back(std::move(s)); return ++calls!=fail_at; }
    bool current_configuration(std::uint8_t& c) noexcept override {c=configuration;return record("configuration");}
    bool open_interface(std::uint8_t n) noexcept override {
        assert(!opened[n]); if(!record("open"+std::to_string(n)))return false; opened[n]=true;return true;
    }
    bool current_alternate(std::uint8_t n,std::uint8_t& a) noexcept override {assert(opened[n]);a=alt[n];return record("get-alt"+std::to_string(n));}
    bool select_alternate(std::uint8_t n,std::uint8_t a) noexcept override {
        assert(opened[n]&&drained);alt[n]=a;
        const bool ok=record("alt"+std::to_string(n)+":"+std::to_string(a));return ok && !(rollback_failure && a==0);
    }
    bool read_rate(RatePlan p,std::uint32_t& r) noexcept override {assert(opened[p.interface_number]);r=rate[p.interface_number];return record("get-rate"+std::to_string(p.interface_number));}
    bool write_rate(RatePlan p,std::uint32_t r) noexcept override {
        assert(opened[p.interface_number]&&drained);rate[p.interface_number]=r;
        return record("rate"+std::to_string(p.interface_number)+":"+std::to_string(r));
    }
    void close_interface(std::uint8_t n) noexcept override {assert(opened[n]&&drained);opened[n]=false;record("close"+std::to_string(n));}
    bool start_io(const audio::Config&,std::uint64_t) noexcept override {
        drained=false;return record("start") && !partial_start;
    }
    void stop_io() noexcept override {record("stop");}
    bool io_drained() const noexcept override {return drained;}
    void restored() {
        assert(!opened[1]&&!opened[2]&&alt[1]==0&&alt[2]==0&&rate[1]==44100&&rate[2]==44100);
    }
};
template<class F> void rejects(F f) {bool threw=false;try{f();}catch(const std::invalid_argument&){threw=true;}assert(threw);}
void descriptors() {
    auto c=usb::parse_configuration(fixture());
    const auto p=plan();assert(p.rate_control_count()==2&&p.interface_control_count()==2);
    assert(p.audio_config().capture.audio.channels==2&&p.audio_config().capture.audio.max_packet_bytes==294);
    assert(make_plan(usb::parse_configuration(fixture(false)),usb::Speed::full,{1,1,0x81},{2,1,2},48000).rate_control_count()==0);
    rejects([&]{make_plan(c,usb::Speed::unknown,{1,1,0x81},{2,1,2},48000);});
    rejects([&]{make_plan(c,usb::Speed::full,{1,1,0x81},{2,1,2},44100);});
    rejects([&]{make_plan(c,usb::Speed::full,{1,0,0x81},{2,1,2},48000);});
    rejects([&]{make_plan(c,usb::Speed::full,{1,1,0x81},{1,0,2},48000);});
    rejects([&]{make_plan(c,usb::Speed::full,{1,1,0x81},{2,1,0x82},48000);});
    auto vendor=c;vendor.alternates[1].interface_class=255;
    rejects([&]{make_plan(vendor,usb::Speed::full,{1,1,0x81},{2,1,2},48000);});
    // Mutate format width, bit precision, rate count, endpoint control/pitch/lock.
    for (std::size_t index=0;index<c.descriptors.size();++index) {
        if (c.descriptors[index].bytes[1]==0x24&&c.descriptors[index].bytes[2]==2) {
            for(auto field : {3,4,5,6,7,8,9}) {
                auto bad=c;bad.descriptors[index].bytes[field]=0;
                rejects([&]{make_plan(bad,usb::Speed::full,{1,1,0x81},{2,1,2},48000);});
            }
        }
        if(c.descriptors[index].bytes[1]==0x25) {
            for(auto field : {4,5,6}) {
                auto bad=c;bad.descriptors[index].bytes[field]=1;
                rejects([&]{make_plan(bad,usb::Speed::full,{1,1,0x81},{2,1,2},48000);});
            }
        }
    }
    // Every structural truncation must fail before a plan exists.
    auto bytes=fixture();
    for(std::size_t n=0;n<bytes.size();++n) {
        bool failed=false;try{usb::parse_configuration({bytes.begin(),bytes.begin()+n});}catch(const usb::ParseError&){failed=true;}assert(failed);
    }
}
void lifecycle() {
    Fake f;Session s(plan(),f);assert(s.prepare());assert(s.state()==State::ready);
    const std::vector<std::string> prepare{"configuration","open1","get-alt1","open2","get-alt2","alt1:1","alt2:1","get-rate1","get-rate2","rate1:48000","rate2:48000"};
    assert(f.log==prepare);assert(!s.prepare());assert(!s.start(0));assert(s.start(100));assert(!s.start(100));
    s.stop();assert(s.state()==State::draining);auto before=f.log.size();s.stop();s.poll();assert(f.log.size()==before);
    assert(f.opened[1]&&f.opened[2]);f.drained=true;s.poll();assert(s.state()==State::idle);f.restored();
    std::vector<std::string> tail(f.log.end()-6,f.log.end());
    assert((tail==std::vector<std::string>{"rate2:44100","rate1:44100","alt2:0","alt1:0","close2","close1"}));
    assert(s.prepare());s.stop();assert(s.state()==State::idle);f.restored();
}
void failures() {
    // Each preparation operation fails in turn. Mutation failures intentionally
    // change fake state before failing, exercising conservative rollback.
    for(int failure=1;failure<=11;++failure) {
        Fake f;f.fail_at=failure;Session s(plan(),f);assert(!s.prepare());assert(s.state()==State::failed);f.restored();
        f.fail_at=-1;assert(s.prepare());s.stop();f.restored();
    }
    Fake f;f.configuration=2;Session s(plan(),f);assert(!s.prepare());assert(f.log.size()==1);f.restored();
    Fake bad;Session t(plan(),bad);assert(t.prepare());bad.rollback_failure=true;t.stop();assert(t.state()==State::uncertain);assert(t.rollback_failed());assert(!t.prepare());
    Fake partial;partial.partial_start=true;Session u(plan(),partial);assert(u.prepare());assert(!u.start(100));assert(u.state()==State::draining);
    assert(partial.opened[1]);partial.drained=true;u.poll();assert(u.state()==State::failed&&u.error()==Error::start);partial.restored();
    Fake hs;Session v(plan(usb::Speed::high),hs);assert(v.prepare());assert(!v.start(101));assert(v.start(104));v.stop();hs.drained=true;v.poll();hs.restored();
}
}
int main(){descriptors();lifecycle();failures();std::cout<<"USB session descriptor/transaction/drain tests passed (no USB I/O)\n";}
