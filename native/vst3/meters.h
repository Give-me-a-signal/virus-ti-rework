#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
namespace VirusRework {
// Display telemetry only. Never serialized as sound state or emitted as automation.
struct MeterSnapshot {
 std::array<float,3> usbPeak{};
 std::array<float,16> midiActivity{};
};
// One capture producer; any number of non-destructive UI/API readers.
class USBMeters {
 std::array<std::atomic<float>,3> levels{};
 std::atomic<int64_t> updated{0};
 static int64_t now() noexcept {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
public:
 USBMeters(){for(auto& v:levels)v.store(0);}
 void clear() noexcept {for(auto& v:levels)v.store(0);updated.store(0);}
 void push(const float* raw,unsigned frames) noexcept {
  if(!frames||!raw)return;
  const auto decay=float(std::exp(-double(frames)/(48000*.25)));
  for(unsigned bus=0;bus<3;++bus){float peak=0;const unsigned channel=(bus*2+2)%6;
   for(unsigned f=0;f<frames;++f)for(unsigned c=0;c<2;++c){float sample=raw[f*6+channel+c];if(std::isfinite(sample))peak=std::max(peak,std::abs(sample));}
   levels[bus].store(std::max(peak,levels[bus].load(std::memory_order_relaxed)*decay),std::memory_order_relaxed);
  }updated.store(now(),std::memory_order_release);
 }
 std::array<float,3> read(bool connected)const noexcept {
  std::array<float,3> result{};const auto age=now()-updated.load(std::memory_order_acquire);
  if(connected&&age<500)for(unsigned b=0;b<3;++b)result[b]=levels[b].load(std::memory_order_relaxed);
  return result;
 }
};
// Per-part MIDI note-on velocity peak, linearly decaying over one second.
// This intentionally has no dB unit: the original Access strip is MIDI activity.
class MIDIActivity {
 std::array<std::atomic<uint64_t>,16> events{};
 static uint64_t now() noexcept {return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
 static float value(uint64_t event,uint64_t at) noexcept {auto stamp=event>>8;return stamp&&at>=stamp?std::max(0.f,float(event&255)/127.f-float(at-stamp)/1000.f):0.f;}
public:
 MIDIActivity(){clear();}
 void clear() noexcept {for(auto& e:events)e.store(0);}
 void midi(const uint8_t* bytes,size_t size) noexcept {
  if(size!=3)return;const unsigned part=bytes[0]&15,status=bytes[0]&0xf0;
  if(status==0xb0&&(bytes[1]==120||bytes[1]==123)){events[part].store(0);return;}
  if(status!=0x90||!bytes[2]||bytes[2]>127)return;
  const auto at=now();const unsigned peak=std::max(unsigned(std::lround(value(events[part].load(),at)*127)),unsigned(bytes[2]));events[part].store((at<<8)|peak);
 }
 std::array<float,16> read(bool connected)const noexcept {std::array<float,16> result{};const auto at=now();if(connected)for(unsigned p=0;p<16;++p)result[p]=value(events[p].load(),at);return result;}
};
inline float meterFraction(float linear) noexcept {return std::isfinite(linear)&&linear>0?std::clamp((20.f*std::log10(linear)+60.f)/60.f,0.f,1.f):0.f;}
}
