#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace VirusRework {
// Same validated MIDI contract for physical USB and the local OsTIrus engine.
class DeviceSession {
public:
 virtual ~DeviceSession()=default;
 virtual void send(const std::vector<uint8_t>&)=0;
 virtual std::vector<uint8_t> single(uint8_t,const std::function<void()>& idle={})=0;
 virtual std::vector<uint8_t> multi()=0;
 virtual void saveTraffic(const std::string&)=0;
};
using EmulatorAudio = void(*)(void*,const float*,unsigned);
using CreateEmulator = DeviceSession*(*)(const char*,EmulatorAudio,void*,char*,unsigned);
}
