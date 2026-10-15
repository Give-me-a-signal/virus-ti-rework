#pragma once
#include "automation_queue.h"
#include "../device-bridge/include/virus/device_bridge.hpp"
#include <algorithm>
namespace VirusRework {
struct PerformanceEvent {
 uint64_t serial=0,generation=0;
 double frame=0;
 std::array<uint8_t,3> bytes{};
 bool clock=false;
 unsigned priority=1,order=0;
};
// Single audio producer, single control-worker consumer. Publication occurs
// only at endBlock so the consumer can sort every completed block by time.
class PerformanceQueue {
 virus::bridge::Spsc<PerformanceEvent,4096> queue_;
 std::atomic<uint64_t> committed_{0},queued_{0};
 AutomationQueue automation_;
 struct Control{std::array<uint8_t,3> bytes{};unsigned offset=0;bool waiting=false;};
 std::array<Control,2048> controls_{};
 uint64_t serial_=0,generation_=0;
 unsigned samples_=0,order_=0;
 double start_=0,step_=1;
 bool enabled_=false,locked_=false;
public:
 std::atomic<uint64_t> overflow{0},dropped{0},coalesced{0};
 std::atomic<bool> panic{false};
 void begin(uint64_t generation,bool enabled,bool locked,double start,double step,unsigned samples,double rate)noexcept{
  if(generation!=generation_||!enabled){automation_.reset();for(auto& c:controls_)c.waiting=false;}
  generation_=generation;enabled_=enabled;locked_=locked;start_=start;step_=step;samples_=samples;++serial_;order_=0;automation_.refill(samples,rate);
 }
 bool publish(unsigned offset,const std::array<uint8_t,3>& bytes,bool clock=false,unsigned priority=1,double shift=0)noexcept{
  if(!enabled_){++dropped;return false;}
  const bool off=(bytes[0]&0xf0)==0x80||((bytes[0]&0xf0)==0x90&&bytes[2]==0)||((bytes[0]&0xf0)==0xb0&&(bytes[1]==64||bytes[1]>=120));
  if((!off&&queued_.load(std::memory_order_acquire)>=3840)||queued_.load()>=4096){++overflow;if(off)panic=true;return false;}
  PerformanceEvent e{serial_,generation_,locked_?start_+offset*step_+shift:-1,bytes,clock,priority,order_++};
  queued_.fetch_add(1,std::memory_order_relaxed);
  if(!queue_.push(e)){queued_.fetch_sub(1);++overflow;if(off)panic=true;return false;}return true;
 }
 bool submit(unsigned offset,std::array<uint8_t,3> bytes)noexcept{
  if(offset>=samples_||!enabled_)return false;
  auto type=bytes[0]&0xf0;if(type!=0x80&&type!=0x90&&type!=0xb0){++dropped;return false;}
  if(type==0xb0&&bytes[1]<120&&bytes[1]!=64){auto id=(bytes[0]&15)*128+bytes[1];auto& c=controls_[id];
   if(c.waiting)++coalesced;else if(!automation_.push(id)){++overflow;return false;}
   c={bytes,offset,true};return true;
  }return publish(offset,bytes);
 }
 void end()noexcept{
  automation_.drain(64,[&](uint32_t id){auto& c=controls_[id];if(!c.waiting)return false;c.waiting=false;return publish(std::min(c.offset,samples_?samples_-1:0),c.bytes,false,0);});
  committed_.store(serial_,std::memory_order_release);
 }
 size_t take(PerformanceEvent* out,size_t max)noexcept{
  size_t n=0;while(n<max){const auto* e=queue_.front();if(!e||e->serial>committed_.load(std::memory_order_acquire))break;out[n++]=*e;queue_.pop();queued_.fetch_sub(1,std::memory_order_release);}return n;
 }
 static bool before(const PerformanceEvent& a,const PerformanceEvent& b)noexcept{
  if(a.serial!=b.serial)return a.serial<b.serial;if(a.frame!=b.frame)return a.frame<b.frame;if(a.priority!=b.priority)return a.priority<b.priority;return a.order<b.order;
 }
};
}
