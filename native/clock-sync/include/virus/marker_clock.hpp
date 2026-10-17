#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <limits>

namespace virus::clock {
enum class SyncState { inactive, preparing, calibrating, locked, degraded, error };
inline const char* syncName(SyncState s)noexcept{
 switch(s){case SyncState::inactive:return "inactive";case SyncState::preparing:return "preparing";case SyncState::calibrating:return "calibrating";case SyncState::locked:return "locked";case SyncState::degraded:return "degraded";default:return "error";}
}
// E139/E141 in TI Revive: a ping resets the private 14-bit sample clock at an
// isolated right/left pair on USB2. Independent 48 kHz detector; no amplitude
// constant or clock conversion is imported from the 44.1 kHz implementation.
// feed() has one capture producer; arm/disarm may run on the control worker.
class MarkerDetector {
 std::atomic<uint64_t> command_{0};
 std::atomic<uint64_t> published_{0};
 uint64_t seen_=0,quiet_=0,frame_=0,candidate_=0;
 unsigned stage_=0,after_=0;
 float amplitude_=0;
 bool armed_=false;
public:
 std::atomic<int64_t> marker{-1};
 std::atomic<uint64_t> lastSound{0};
 int64_t validMarker()const noexcept{
  auto token=command_.load(std::memory_order_acquire);
  if(!token||(token&1)||published_.load(std::memory_order_acquire)!=token)return -1;
  return marker.load(std::memory_order_acquire);
 }
 void arm()noexcept{marker=-1;auto old=command_.load();while(!command_.compare_exchange_weak(old,(old+2)&~uint64_t(1),std::memory_order_release)){};}
 void disarm()noexcept{command_.fetch_or(1,std::memory_order_release);}
 void feed(const float* raw,unsigned frames,uint64_t first)noexcept{
  auto token=command_.load(std::memory_order_acquire);
  if(token!=seen_){seen_=token;armed_=token&&!(token&1);stage_=0;after_=0;}
  if(first!=frame_){quiet_=0;stage_=0;}frame_=first;
  for(unsigned f=0;f<frames;++f,++frame_){
   const auto* s=raw+f*6;bool quiet=true,otherQuiet=true;
   for(unsigned c=0;c<6;++c){bool z=std::isfinite(s[c])&&std::abs(s[c])<=1e-7f;quiet&=z;if(c<4)otherQuiet&=z;}
   const bool right=otherQuiet&&std::abs(s[4])<=1e-7f&&std::isfinite(s[5])&&std::abs(s[5])>1e-7f;
   const bool left=otherQuiet&&std::abs(s[5])<=1e-7f&&std::isfinite(s[4])&&std::abs(s[4]-amplitude_)<=1e-7f;
   if(armed_){
    if(stage_==0&&quiet_>=48000&&right){candidate_=frame_;amplitude_=s[5];stage_=1;}
    else if(stage_==1){if(left){stage_=2;after_=0;}else stage_=0;}
    else if(stage_==2){if(!quiet)stage_=0;else if(++after_==64){marker.store(int64_t(candidate_),std::memory_order_release);published_.store(seen_,std::memory_order_release);armed_=false;stage_=0;}}
   }
   if(quiet)++quiet_;else{quiet_=0;lastSound.store(frame_,std::memory_order_relaxed);}
  }
 }
};

// One audio consumer. Frames are the 48 kHz capture domain, independent of
// project position, which may loop or seek. No F5 transmission is allowed
// until a hardware-confirmed calibration enables the private transport.
struct MarkerTimeline {
 static constexpr double deviceRate=48000,transmitLead=2048,timelineOffset=4096;
 double rate=48000,position=0,fraction=0,nextTick=0;
 unsigned maxBlock=256;
 bool valid=false;
 uint64_t generation=0;
 void setup(double r,unsigned b)noexcept{rate=r;maxBlock=b;valid=false;fraction=0;}
 double ratio()const noexcept{return deviceRate/rate;}
 unsigned safety()const noexcept{return std::max(1024u,2*unsigned(std::ceil(maxBlock*ratio())));}
 unsigned latencyDevice()const noexcept{return unsigned(transmitLead)+safety()+32;}
 unsigned latencyHost()const noexcept{return unsigned(std::ceil(latencyDevice()*rate/deviceRate));}
 double begin(uint64_t newest,uint64_t epoch)noexcept{
  double target=double(newest)-timelineOffset;
  if(!valid||generation!=epoch){position=target;nextTick=position;fraction=0;valid=true;generation=epoch;}
  double err=target-position;
  if(std::abs(err)>512){position=target;nextTick+=err;}
  else if(std::abs(err)>256){double step=err>0?1:-1;position+=step;nextTick+=step;}
  return position+timelineOffset+transmitLead-latencyDevice();
 }
 void advance(unsigned samples)noexcept{fraction+=samples*ratio();auto whole=std::floor(fraction);position+=whole;fraction-=whole;}
 static bool stamp(double desired,int64_t anchor,uint64_t newest,uint16_t& raw)noexcept{
  if(anchor<0||!std::isfinite(desired)||desired<double(newest)||desired-double(newest)>=8192||desired>=double(std::numeric_limits<int64_t>::max()))return false;
  auto rounded=int64_t(std::llround(desired));
  if(uint64_t(rounded)<newest||uint64_t(rounded)-newest>=8192)return false;
  auto delta=rounded-anchor;raw=uint16_t(uint64_t(delta)&16383);return true;
 }
};
}
