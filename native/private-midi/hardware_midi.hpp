#pragma once
#import <Foundation/Foundation.h>
#import <CoreMIDI/CoreMIDI.h>
#include <array>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "private_usb.hpp"
#include "dump_transaction.h"
#include "../vst3/device_profile.h"
#include "../device-identity/include/virus/device_identity.hpp"
#include "../vst3/midi_safety.h"
namespace virus::hardware {
inline void check(OSStatus s,const char* op){if(s)throw std::runtime_error(std::string(op)+": "+std::to_string(s));}
inline std::string midiName(MIDIObjectRef o){CFStringRef s=nullptr;MIDIObjectGetStringProperty(o,kMIDIPropertyDisplayName,&s);if(!s)return {};std::string r=[(__bridge NSString*)s UTF8String];CFRelease(s);return r;}
class Midi {
 MIDIClientRef client=0;MIDIPortRef input=0,output=0;MIDIEndpointRef destination=0,source=0,externalSource=0;
 bool snowTest=false;
 std::mutex requestLock;
 std::mutex lock;std::array<std::vector<uint8_t>,2> assembling;std::vector<std::vector<uint8_t>> messages;std::vector<std::string> origins;
 std::unique_ptr<virus::private_midi::PrivateUSB> privateUSB;
 static void callback(const MIDIPacketList* list,void* context,void* sourceContext){auto* self=static_cast<Midi*>(context);std::lock_guard<std::mutex> guard(self->lock);auto& assembly=self->assembling[sourceContext?1:0];auto* p=&list->packet[0];for(UInt32 n=0;n<list->numPackets;n++,p=MIDIPacketNext(p))for(UInt16 i=0;i<p->length;i++){uint8_t b=p->data[i];if(b>=0xf8)continue;if(b==0xf0)assembly.clear();if(b==0xf0||!assembly.empty()){if(assembly.size()<65536)assembly.push_back(b);if(b==0xf7){self->messages.push_back(assembly);self->origins.push_back("CoreMIDI public");assembly.clear();}}}}
public:
 explicit Midi(bool snow=false):snowTest(snow){for(ItemCount i=0;i<MIDIGetNumberOfDestinations();i++){auto e=MIDIGetDestination(i);if(virus::identity::midiPort(midiName(e))==virus::identity::MidiPort::plugin){if(destination)throw std::runtime_error("Ambiguous MIDI destination");destination=e;}}for(ItemCount i=0;i<MIDIGetNumberOfSources();i++){auto e=MIDIGetSource(i);if(virus::identity::midiPort(midiName(e))==virus::identity::MidiPort::plugin){if(source)throw std::runtime_error("Ambiguous MIDI source");source=e;}if(virus::identity::midiPort(midiName(e))==virus::identity::MidiPort::external){if(externalSource)throw std::runtime_error("Ambiguous external MIDI source");externalSource=e;}}if(!source||!destination)throw std::runtime_error("Virus TI plugin MIDI port absent; a compatible CoreMIDI driver/Plugin I/O source or destination is missing. Check Audio MIDI Setup > MIDI Studio and report driver/version (issue #8).");check(MIDIClientCreate(CFSTR("Virus bench no-firmware"),nullptr,nullptr,&client),"MIDI client");check(MIDIInputPortCreate(client,CFSTR("Virus bench input"),callback,this,&input),"MIDI input");check(MIDIOutputPortCreate(client,CFSTR("Virus bench output"),&output),"MIDI output");check(MIDIPortConnectSource(input,source,nullptr),"MIDI connect");if(externalSource)check(MIDIPortConnectSource(input,externalSource,reinterpret_cast<void*>(1)),"External MIDI receive connect");}
 ~Midi(){privateUSB.reset();if(input){MIDIPortDisconnectSource(input,source);if(externalSource)MIDIPortDisconnectSource(input,externalSource);MIDIPortDispose(input);}if(output)MIDIPortDispose(output);if(client)MIDIClientDispose(client);}
 void usePrivate(bool snow=false){privateUSB=std::make_unique<virus::private_midi::PrivateUSB>(snow);privateUSB->startReader([this](const std::vector<uint8_t>& m){std::lock_guard<std::mutex> g(lock);messages.push_back(m);origins.push_back("hidden interface IN");});}
 uint64_t registryID()const{return privateUSB?privateUSB->registryID:0;}
 void savePrivate(NSString* dir){if(privateUSB){privateUSB->saveTraffic(dir);if(!privateUSB->readerError().empty())throw std::runtime_error(privateUSB->readerError());}}
 void request(uint8_t command,uint8_t program=0x40){if(command!=0x30&&command!=0x31&&command!=0x35)throw std::runtime_error("Read-request allowlist");send({0xf0,0,0x20,0x33,1,0x10,command,0,program,0xf7});}
 void send(const std::vector<uint8_t>& data){
  if(snowTest&&VirusRework::snowPlaybackTestBuild)VirusRework::validateSnowTestMidi(data);else VirusRework::validateMidi(data);
  if(privateUSB){privateUSB->sendMessage(data);return;}
  std::array<uint8_t,4096> storage{};auto* list=(MIDIPacketList*)storage.data();auto* p=MIDIPacketListInit(list);if(!MIDIPacketListAdd(list,sizeof(storage),p,0,data.size(),data.data()))throw std::runtime_error("MIDI packet too large");check(MIDISend(output,destination,list),"MIDI send");
 }
 std::vector<std::vector<uint8_t>> received(){std::lock_guard<std::mutex> guard(lock);return messages;}
 std::string origin(size_t index){std::lock_guard<std::mutex> guard(lock);return origins.at(index);}
 std::vector<uint8_t> editDump(uint8_t command,uint8_t part,const std::function<void()>& idle={}){
  std::lock_guard<std::mutex> serial(requestLock);size_t before=0;
  return readDump([&]{before=received().size();request(command,part);},[&]{
   auto all=received();for(size_t i=before;i<all.size();++i){const auto& m=all[i];
    const bool single=command==0x30;const size_t size=single?524:267;
    if(m.size()!=size||m[0]!=0xf0||m[1]!=0||m[2]!=0x20||m[3]!=0x33||m[4]!=1||m[5]!=16||m[6]!=(single?0x10:0x11)||m[7]!=0||m[8]!=(single?part:127)||m.back()!=0xf7)continue;
    for(size_t j=1;j<m.size()-1;++j)if(m[j]>=128)throw std::runtime_error("Invalid dump data");
    unsigned sum=0;for(size_t j=5;j<265;++j)sum+=m[j];if((sum&127)!=m[265])throw std::runtime_error("Dump checksum failed");
    if(single){sum=0;for(size_t j=5;j<522;++j)sum+=m[j];if((sum&127)!=m[522])throw std::runtime_error("Single checksum failed");}
    return m;
   }return std::vector<uint8_t>{};
  },[&](unsigned ms){for(unsigned t=0;t<ms;t+=10){if(idle)idle();std::this_thread::sleep_for(std::chrono::milliseconds(10));}},[]{return std::chrono::steady_clock::now();});
 }
 std::vector<uint8_t> single(uint8_t part,const std::function<void()>& idle={}){if(part>=16&&part!=64)throw std::runtime_error("Invalid edit-buffer part");return editDump(0x30,part,idle);}
 std::vector<uint8_t> multi(){return editDump(0x31,0);}
};
}
