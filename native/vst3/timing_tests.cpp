#include "../clock-sync/include/virus/marker_clock.hpp"
#include "automation_queue.h"
#include "timed_midi.h"
#include "capture_fifo.h"
#include "performance_queue.h"
#include <cassert>
#include <memory>
#include <iostream>
#include <cstdlib>
#include <new>
static bool allocationProbe=false;
static unsigned realtimeAllocations=0;
void* operator new(std::size_t n){if(allocationProbe)++realtimeAllocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p)noexcept{std::free(p);}
using namespace VirusRework;
using namespace virus::clock;
int main(){
 MarkerDetector detector;std::vector<float> silence(48000*6);detector.feed(silence.data(),48000,0);detector.arm();
 // Marker crosses callback boundaries; neither an ordinary impulse nor a
 // truncated stereo pattern may establish a clock.
 float right[6]{0,0,0,0,0,.015f},left[6]{0,0,0,0,.015f,0};
 detector.feed(right,1,48000);assert(detector.marker==-1);detector.feed(left,1,48001);
 detector.feed(silence.data(),63,48002);assert(detector.marker==-1);detector.feed(silence.data(),1,48065);assert(detector.marker==48000);
 detector.disarm();detector.feed(silence.data(),48000,48066);detector.arm();
 float other[6]{.1f,0,0,0,0,.015f};detector.feed(other,1,96066);detector.feed(left,1,96067);detector.feed(silence.data(),100,96068);assert(detector.marker==-1);
 // Re-arming after cancellation works, but requires a full quiet interval.
 detector.disarm();detector.feed(silence.data(),48000,96168);detector.arm();detector.feed(right,1,144168);detector.feed(left,1,144169);detector.feed(silence.data(),64,144170);assert(detector.marker==144168);
 uint16_t raw=0;assert(MarkerTimeline::stamp(20010,3626,19900,raw)&&raw==0);
 assert(!MarkerTimeline::stamp(19900,-1,19900,raw));assert(!MarkerTimeline::stamp(19899,0,19900,raw));assert(!MarkerTimeline::stamp(19900+8192,0,19900,raw));
 assert(!MarkerTimeline::stamp(19900+8191.9,0,19900,raw));assert(!MarkerTimeline::stamp(1e100,0,19900,raw));
 for(double rate:{44100.,48000.,96000.})for(unsigned block:{64u,256u,512u}){
  MarkerTimeline timeline;timeline.setup(rate,block);uint64_t newest=480000;double read=timeline.begin(newest,1);
  assert(std::abs(read-(double(newest)-timeline.safety()-32))<1e-8);
  double start=timeline.position;for(unsigned i=0;i<10000;++i)timeline.advance(block);
  assert(std::abs(timeline.position+timeline.fraction-start-10000.*block*48000/rate)<1e-6);
  timeline.begin(newest,2);assert(timeline.generation==2);
  assert(timeline.latencyHost()==unsigned(std::ceil(timeline.latencyDevice()*rate/48000)));
 }
 auto packets=timedPackets({0x90,60,1},16383);assert((packets==std::vector<uint8_t>{0x53,0xf5,127,127,0x29,0x90,60,1}));
 assert(timedBatchPackets({{{0x90,60,1},16383,false},{{0x80,60,0},0,false}}).size()==16);
 bool oversized=false;try{timedBatchPackets(std::vector<TimedMessage>(33,{{0x90,60,1},0,false}));}catch(...){oversized=true;}assert(oversized);
 for(auto command:{ClockCommand::activeSense,ClockCommand::songPosition,ClockCommand::tick,ClockCommand::calibrationPing})assert(!timedPackets(clockMessage(command),3072,true).empty());
 for(const auto& bad:std::vector<std::vector<uint8_t>>{{0xf0,0,0x20,0x33,1,16,0x50,0,0,0xf7},{0xf0,0,0x20,0x33,1,16,0x73,0,0x22,1,0xf7},{0xf8,1},{0xff},{0xf5,1,2}}){
  bool rejected=false;try{timedPackets(bad,0,true);}catch(...){rejected=true;}assert(rejected);
 }
 AutomationQueue limiter;unsigned sent=0;bool waiting[10]{};
 for(unsigned block=0;block<48000/64;++block){limiter.refill(64,48000);for(unsigned id=0;id<10;++id)if(!waiting[id]){assert(limiter.push(id));waiting[id]=true;}sent+=limiter.drain(64,[&](unsigned id){assert(waiting[id]);waiting[id]=false;return true;});}
 assert(sent>=249&&sent<=250);
 auto events=std::make_unique<PerformanceQueue>();std::array<PerformanceEvent,4096> ready{};
 allocationProbe=true;
 events->begin(1,true,true,10000,1,256,48000);
 assert(events->submit(100,{0x90,60,80}));assert(events->submit(10,{0x80,60,0}));
 assert(events->take(ready.data(),ready.size())==0); // incomplete block is invisible
 events->end();allocationProbe=false;assert(realtimeAllocations==0);auto count=events->take(ready.data(),ready.size());assert(count==2);
 std::stable_sort(ready.begin(),ready.begin()+count,PerformanceQueue::before);assert(ready[0].frame==10010&&ready[1].frame==10100);
 // Latest-value coalescing, actual shared limiter, generation cancellation.
 unsigned controls=0;
 for(unsigned block=0;block<750;++block){events->begin(1,true,true,10000+block*64,1,64,48000);assert(events->submit(0,{0xb0,40,1}));assert(events->submit(1,{0xb0,40,2}));events->end();count=events->take(ready.data(),ready.size());controls+=count;for(unsigned i=0;i<count;++i)assert(ready[i].bytes[2]==2);}
 assert(controls>=249&&controls<=252&&events->coalesced>0);
 events->begin(2,true,true,20000,1,64,48000);events->end();assert(events->take(ready.data(),ready.size())==0);
 events->begin(2,true,true,20000,1,64,48000);
 for(unsigned i=0;i<3840;++i)assert(events->submit(0,{0x90,60,80}));assert(!events->submit(0,{0x90,60,80}));
 for(unsigned i=0;i<256;++i)assert(events->submit(0,{0x80,60,0}));assert(!events->submit(0,{0x80,60,0})&&events->panic);
 events->end();assert(events->take(ready.data(),ready.size())==4096);
 events->begin(3,false,false,0,1,64,48000);assert(!events->submit(0,{0x90,60,80}));events->end();assert(events->take(ready.data(),ready.size())==0);
 auto fifo=std::make_unique<CaptureFIFO>();fifo->setup(48000);std::vector<float> input(8192*6);input[1000*6+5]=.015f;input[1001*6+4]=.015f;
 assert(fifo->push(input.data(),8192));fifo->suppressCalibrationMarker(1000);fifo->beginAlignedBlock(1000);std::array<int32_t,6> value{};assert(fifo->pop(value));for(auto v:value)assert(v==0);
 std::cout<<"Marker boundary/rejection/rearm, 14-bit horizon, fractional rates, private clock allowlist, limiter and marker suppression passed\n";
}
