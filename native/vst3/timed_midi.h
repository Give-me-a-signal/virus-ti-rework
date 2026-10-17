#pragma once
#include "midi_safety.h"
#include "../midi-core/include/private_control.hpp"
#include "../midi-core/include/usb_midi1.hpp"
#include <array>
#include <vector>
namespace VirusRework {
enum class ClockCommand { activeSense, songPosition, tick, calibrationPing };
struct TimedMessage {std::vector<uint8_t> bytes;uint16_t stamp;bool clock=false;};
inline std::vector<uint8_t> clockMessage(ClockCommand command,unsigned value=0){
 switch(command){
  case ClockCommand::activeSense:return {0xfe};
  case ClockCommand::songPosition:if(value>16383)throw std::runtime_error("Invalid song position");return {0xf2,uint8_t(value&127),uint8_t((value>>7)&127)};
  case ClockCommand::tick:return {0xf8};
  case ClockCommand::calibrationPing:return {0xf0,0,0x20,0x33,1,16,0x73,0,0x22,0,0xf7};
 }throw std::runtime_error("Invalid clock command");
}
inline std::vector<uint8_t> timedPackets(const std::vector<uint8_t>& message,uint16_t stamp,bool internalClock=false){
 if(internalClock){bool valid=false;for(auto command:{ClockCommand::activeSense,ClockCommand::tick,ClockCommand::calibrationPing})valid|=message==clockMessage(command);
  valid|=message.size()==3&&message[0]==0xf2&&message[1]<128&&message[2]<128;
  if(!valid)throw std::runtime_error("Blocked internal clock message");
 }else validateMidi(message);
 virus::midi::private_control::WordBytes word{};
 if(!virus::midi::private_control::encode_timing_marker(stamp,word))throw std::runtime_error("Invalid private timestamp");
 std::vector<virus::midi::USBMidiPacket> encoded(message.size()/3+1);size_t count=0;
 if(virus::midi::encode_usb_midi1_message(2,message.data(),message.size(),encoded.data(),encoded.size(),count)!=virus::midi::USBMidiResult::ok)throw std::runtime_error("Invalid timed MIDI");
 std::vector<uint8_t> output(word.begin(),word.end());for(size_t i=0;i<count;++i)output.insert(output.end(),encoded[i].begin(),encoded[i].end());return output;
}
inline std::vector<uint8_t> timedBatchPackets(const std::vector<TimedMessage>& batch){
 if(batch.empty()||batch.size()>32)throw std::runtime_error("Invalid timed batch size");
 std::vector<uint8_t> result;
 for(const auto& m:batch){auto words=timedPackets(m.bytes,m.stamp,m.clock);result.insert(result.end(),words.begin(),words.end());}
 return result; // every message validated before any USB submission
}
}
