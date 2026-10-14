#include "meters.h"
#include <cassert>
#include <limits>
#include <thread>
using namespace VirusRework;
int main(){
 USBMeters meters;std::array<float,6*480> audio{};
 // Capture order is USB3,USB1,USB2; no mixing or copying between buses.
 for(unsigned i=0;i<480;++i){audio[i*6]=.25f;audio[i*6+2]=-.5f;audio[i*6+4]=.125f;}
 meters.push(audio.data(),480);auto p=meters.read(true);assert(p[0]==.5f&&p[1]==.125f&&p[2]==.25f);assert((meters.read(false)==std::array<float,3>{}));
 assert(meters.read(true)==p); // Reading API must not consume editor telemetry.
 audio.fill(0);meters.push(audio.data(),480);assert(meters.read(true)[0]<p[0]);
 audio[2]=std::numeric_limits<float>::quiet_NaN();meters.push(audio.data(),480);assert(std::isfinite(meters.read(true)[0]));meters.clear();assert((meters.read(true)==std::array<float,3>{}));
 assert(meterFraction(0)==0&&meterFraction(.001f)<1e-5&&std::abs(meterFraction(.1f)-2.f/3)<1e-5&&meterFraction(1)==1);
 MIDIActivity activity;uint8_t note[3]={0x9f,60,127};activity.midi(note,3);auto a=activity.read(true);assert(a[15]>.99);for(unsigned p=0;p<15;++p)assert(a[p]==0);
 assert((activity.read(false)==std::array<float,16>{}));note[0]=0x90;note[2]=64;activity.midi(note,3);assert(activity.read(true)[0]>.49&&activity.read(true)[0]<.51);
 note[0]=0x81;activity.midi(note,3);assert(activity.read(true)[1]==0);note[0]=0x91;note[2]=0;activity.midi(note,3);assert(activity.read(true)[1]==0);
 note[0]=0xbf;note[1]=123;activity.midi(note,3);assert(activity.read(true)[15]==0);
 std::this_thread::sleep_for(std::chrono::milliseconds(550));assert(activity.read(true)[0]==0);
 note[0]=0x90;note[1]=60;note[2]=127;activity.midi(note,3);activity.clear();assert((activity.read(true)==std::array<float,16>{}));
}
