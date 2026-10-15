#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <stdexcept>
#include "timed_midi.h"
#include <vector>
namespace VirusRework {
// Same validated MIDI contract for physical USB and the local OsTIrus engine.
class DeviceSession {
public:
 virtual ~DeviceSession()=default;
 virtual void send(const std::vector<uint8_t>&)=0;
 virtual void sendTimed(const std::vector<uint8_t>&,uint16_t,bool=false){throw std::runtime_error("Timed transport unavailable");}
 virtual void sendTimedBatch(const std::vector<TimedMessage>& batch){for(const auto& m:batch)sendTimed(m.bytes,m.stamp,m.clock);}
 virtual std::vector<uint8_t> single(uint8_t,const std::function<void()>& idle={})=0;
 virtual std::vector<uint8_t> multi()=0;
 virtual void saveTraffic(const std::string&)=0;
};
using EmulatorAudio = void(*)(void*,const float*,unsigned);
using CreateEmulator = DeviceSession*(*)(const char*,EmulatorAudio,void*,char*,unsigned);
}
