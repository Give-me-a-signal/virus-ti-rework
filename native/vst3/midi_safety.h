#pragma once
#include <algorithm>
#include <vector>
#include <cstdint>
#include <stdexcept>
namespace VirusRework {
inline void validateMidi(const std::vector<uint8_t>& data,bool snowTestMultiMode=false){
  // No firmware command or stored-bank write can pass this boundary.
  if(data.empty())throw std::runtime_error("Empty MIDI");
  if(data[0]==0xf0){if(data.size()<8||data.back()!=0xf7||std::any_of(data.begin()+1,data.end()-1,[](auto v){return v>=128;}))throw std::runtime_error("Require one complete 7-bit SysEx; concatenated commands forbidden");if((data[6]==0x30||data[6]==0x31)&&(data.size()!=10||data[7]!=0))throw std::runtime_error("Only edit-buffer read requests allowed");if(data[6]==0x35&&data.size()!=8&&data.size()!=10)throw std::runtime_error("Invalid global request length");}
  else if(data.size()==3&&(data[1]>=128||data[2]>=128))throw std::runtime_error("Invalid short MIDI data bytes");
  if(data[0]==0xf0){if(data.size()<8||data[1]!=0||data[2]!=0x20||data[3]!=0x33||data[4]!=1)throw std::runtime_error("Vendor allowlist");auto cmd=data[6];bool read=cmd==0x30||cmd==0x31||cmd==0x35;bool edit=cmd==0x10&&(data.size()==524||data.size()==267)&&data[7]==0&&(data[8]<16||data[8]==64);bool multiEdit=cmd==0x11&&data.size()==267&&data[7]==0&&data[8]==127;bool control=(cmd==0x70||cmd==0x71||cmd==0x6e||cmd==0x6f)&&data.size()==11;bool route=data.size()==11&&cmd==0x72&&data[7]<16&&((data[8]==41&&data[9]<=17)||data[8]==32||data[8]==33);bool mode=data.size()==11&&cmd==0x73&&data[7]==0&&data[8]==122&&(data[9]==0||data[9]==3||(snowTestMultiMode&&data[9]==1));bool usbIO=data.size()==11&&cmd==0x73&&data[7]==0&&data[8]==9&&data[9]<=1;bool standaloneCable=data.size()==11&&cmd==0x73&&data[7]==0&&(data[8]>=58&&data[8]<=60)&&data[9]==1;if(!read&&!edit&&!multiEdit&&!control&&!route&&!mode&&!usbIO&&!standaloneCable)throw std::runtime_error("Blocked SysEx outside volatile patch/read allowlist");}
  else if(data.size()!=3||((data[0]&0xf0)!=0x80&&(data[0]&0xf0)!=0x90&&(data[0]&0xf0)!=0xb0))throw std::runtime_error("Blocked MIDI status");
}
inline void validateSnowTestMidi(const std::vector<uint8_t>& data){
 validateMidi(data,true);
 if(data[0]!=0xf0){if((data[0]&15)>=4)throw std::runtime_error("Snow test channel outside four parts");return;}
 const auto cmd=data[6];
 if(cmd==0x35)throw std::runtime_error("Unneeded Snow global request blocked");
 if((cmd==0x30||cmd==0x10)&&(data[8]>=4&&data[8]!=64))throw std::runtime_error("Snow test buffer outside four parts");
 if(cmd==0x10&&(data.size()!=524||data[9]<12))throw std::runtime_error("Legacy Snow project patch blocked before write");
 if(cmd==0x73&&(data[8]!=122||(data[9]!=0&&data[9]!=1)))throw std::runtime_error("Unverified Snow global control blocked");
 if((cmd==0x70||cmd==0x71||cmd==0x6e||cmd==0x6f||cmd==0x72)&&data[7]>=4)throw std::runtime_error("Snow control outside four parts");
 if(cmd==0x72&&(data[8]!=41||(data[9]!=10&&data[9]!=13)))throw std::runtime_error("Snow test supports USB1/2 stereo routing only");
}
inline void validateDeviceMidi(const std::vector<uint8_t>& data,bool snow){
 // Model restrictions apply identically in standard and labelled test builds.
 if(snow)validateSnowTestMidi(data);else validateMidi(data);
}
}
