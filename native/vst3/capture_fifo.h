#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace VirusRework {
// One CoreAudio producer and one DAW consumer, never shared sample clocks.
// 64-tap, 1024-phase windowed-sinc resampling; occupancy controls slow drift.
class CaptureFIFO {
 static constexpr uint64_t capacity=32768;
 static constexpr unsigned taps=64, phases=1024;
 std::array<std::array<float,6>,capacity> data{};
 std::array<std::array<double,taps>,phases> kernel{};
 alignas(64) std::atomic<uint64_t> written{0},read{0};
 uint64_t cursor=0;
 double phase=0,step=1,correction=0;
 bool primed=false;
 std::atomic<unsigned> requestedTarget{2048};
 std::atomic<int64_t> suppressedMarker{-1};
 unsigned target=2048;
public:
 std::atomic<uint64_t> overruns{0},underruns{0},rendered{0};
 void setTarget(unsigned frames) noexcept {requestedTarget.store(std::clamp(frames,2048u,16384u));}
 void setup(double rate) { // consumer quiescent
  step=48000./rate;
  const double cutoff=.94*std::min(1.,rate/48000.);
  constexpr double pi=3.14159265358979323846;
  for(unsigned p=0;p<phases;p++){
   double sum=0;
   for(unsigned k=0;k<taps;k++){
    const double x=double(k)-31-double(p)/phases;
    const double sinc=std::abs(x)<1e-12?cutoff:std::sin(pi*cutoff*x)/(pi*x);
    const double window=.42-.5*std::cos(2*pi*k/(taps-1))+.08*std::cos(4*pi*k/(taps-1));
    sum+=(kernel[p][k]=sinc*window);
   }
   for(auto& v:kernel[p])v/=sum;
  }
  resetConsumer();
 }
 void resetConsumer() noexcept {cursor=written.load(std::memory_order_acquire);read.store(cursor,std::memory_order_release);phase=0;correction=0;primed=false;}
 uint64_t newest()const noexcept{return written.load(std::memory_order_acquire);}
 void suppressCalibrationMarker(int64_t frame)noexcept{suppressedMarker.store(frame,std::memory_order_release);}
 double position()const noexcept{return double(cursor)+phase+31;}
 double increment()const noexcept{return step*(1+correction);}
 bool ready()const noexcept{return primed;}
 bool push(const float* src,unsigned count) noexcept {
  auto w=written.load(std::memory_order_relaxed);auto r=read.load(std::memory_order_acquire);
  if(count>capacity-(w-r)){++overruns;return false;}
  for(unsigned f=0;f<count;f++)for(unsigned c=0;c<6;c++)data[(w+f)&(capacity-1)][c]=std::isfinite(src[f*6+(c+2)%6])?src[f*6+(c+2)%6]:0;
  written.store(w+count,std::memory_order_release);
  return true;
 }
 // Same sinc kernel and six-channel order, now driven by the shared device
 // timeline. desired is the center sample, so include the kernel look-ahead.
 void beginAlignedBlock(double desired)noexcept{
  const auto w=written.load(std::memory_order_acquire);
  double start=desired-31;
  if(!std::isfinite(start)||start<0||start+taps>=double(w)){primed=false;return;}
  double err=start-(double(cursor)+phase);
  if(!primed||std::abs(err)>2048){cursor=uint64_t(start);phase=start-double(cursor);correction=0;primed=true;}
  else correction=std::clamp(err/(64.*std::max(1.,step*256.)),-.002,.002);
  read.store(cursor,std::memory_order_release);
 }
 void beginBlock(bool active) noexcept {
  auto desiredTarget=requestedTarget.load();if(target!=desiredTarget){target=desiredTarget;resetConsumer();}
  auto w=written.load(std::memory_order_acquire);auto available=w-cursor;
  if(!active){resetConsumer();return;}
  if(!primed){if(available<target+taps)return;cursor=w-target;read.store(cursor,std::memory_order_release);primed=true;}
  // Hard re-prime after host suspension rather than play old buffered notes.
  if(available>std::min<uint64_t>(target*4,capacity-taps)){resetConsumer();return;}
  double desired=std::clamp((double(available)-target)/target*.002,-.002,.002);
  correction+=.001*(desired-correction);
 }
 bool pop(std::array<int32_t,6>& out) noexcept {
  if(!primed)return false;
  if(written.load(std::memory_order_acquire)-cursor<taps+unsigned(std::ceil(step))+1){++underruns;primed=false;return false;}
  const auto& coeff=kernel[std::min(unsigned(phase*phases),phases-1)];
  for(unsigned c=0;c<6;c++){
   double v=0;const auto marker=suppressedMarker.load(std::memory_order_acquire);
   for(unsigned k=0;k<taps;k++)if(!(marker>=0&&(c==2||c==3)&&(cursor+k==uint64_t(marker)||cursor+k==uint64_t(marker)+1)))v+=data[(cursor+k)&(capacity-1)][c]*coeff[k];
   out[c]=int32_t(std::clamp(v*2147483648.,-2147483648.,2147483647.));
  }
  phase+=step*(1+correction);auto advance=uint64_t(phase);cursor+=advance;phase-=advance;
  read.store(cursor,std::memory_order_release);++rendered;return true;
 }
 unsigned latency(double rate)const noexcept{return unsigned(std::ceil(requestedTarget.load()*rate/48000.));}
};
}
