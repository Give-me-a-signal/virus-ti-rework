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
#include "../vst3/midi_safety.h"
namespace virus::hardware {
inline void check(OSStatus s,const char* op){if(s)throw std::runtime_error(std::string(op)+": "+std::to_string(s));}
inline std::string midiName(MIDIObjectRef o){CFStringRef s=nullptr;MIDIObjectGetStringProperty(o,kMIDIPropertyDisplayName,&s);if(!s)return {};std::string r=[(__bridge NSString*)s UTF8String];CFRelease(s);return r;}
class Midi {
 MIDIClientRef client=0;MIDIPortRef input=0,output=0;MIDIEndpointRef destination=0,source=0,externalSource=0;
 std::mutex lock;std::array<std::vector<uint8_t>,2> assembling;std::vector<std::vector<uint8_t>> messages;std::vector<std::string> origins;
 std::unique_ptr<virus::private_midi::PrivateUSB> privateUSB;
 static void callback(const MIDIPacketList* list,void* context,void* sourceContext){auto* self=static_cast<Midi*>(context);std::lock_guard<std::mutex> guard(self->lock);auto& assembly=self->assembling[sourceContext?1:0];auto* p=&list->packet[0];for(UInt32 n=0;n<list->numPackets;n++,p=MIDIPacketNext(p))for(UInt16 i=0;i<p->length;i++){uint8_t b=p->data[i];if(b>=0xf8)continue;if(b==0xf0)assembly.clear();if(b==0xf0||!assembly.empty()){if(assembly.size()<65536)assembly.push_back(b);if(b==0xf7){self->messages.push_back(assembly);self->origins.push_back("CoreMIDI public");assembly.clear();}}}}
public:
 Midi(){for(ItemCount i=0;i<MIDIGetNumberOfDestinations();i++){auto e=MIDIGetDestination(i);if(midiName(e)=="Virus TI USB Plugin I/O"){if(destination)throw std::runtime_error("Ambiguous MIDI destination");destination=e;}}for(ItemCount i=0;i<MIDIGetNumberOfSources();i++){auto e=MIDIGetSource(i);if(midiName(e)=="Virus TI USB Plugin I/O"){if(source)throw std::runtime_error("Ambiguous MIDI source");source=e;}if(midiName(e)=="Virus TI USB External I/O")externalSource=e;}if(!source||!destination)throw std::runtime_error("Virus plugin MIDI port absent");check(MIDIClientCreate(CFSTR("Virus bench no-firmware"),nullptr,nullptr,&client),"MIDI client");check(MIDIInputPortCreate(client,CFSTR("Virus bench input"),callback,this,&input),"MIDI input");check(MIDIOutputPortCreate(client,CFSTR("Virus bench output"),&output),"MIDI output");check(MIDIPortConnectSource(input,source,nullptr),"MIDI connect");if(externalSource)check(MIDIPortConnectSource(input,externalSource,reinterpret_cast<void*>(1)),"External MIDI receive connect");}
 ~Midi(){privateUSB.reset();if(input){MIDIPortDisconnectSource(input,source);if(externalSource)MIDIPortDisconnectSource(input,externalSource);MIDIPortDispose(input);}if(output)MIDIPortDispose(output);if(client)MIDIClientDispose(client);}
 void usePrivate(){privateUSB=std::make_unique<virus::private_midi::PrivateUSB>();privateUSB->startReader([this](const std::vector<uint8_t>& m){std::lock_guard<std::mutex> g(lock);messages.push_back(m);origins.push_back("hidden interface IN");});}
 uint64_t registryID()const{return privateUSB?privateUSB->registryID:0;}
 void savePrivate(NSString* dir){if(privateUSB){privateUSB->saveTraffic(dir);if(!privateUSB->readerError().empty())throw std::runtime_error(privateUSB->readerError());}}
 void request(uint8_t command,uint8_t program=0x40){if(command!=0x30&&command!=0x31&&command!=0x35)throw std::runtime_error("Read-request allowlist");send({0xf0,0,0x20,0x33,1,0x10,command,0,program,0xf7});}
 void send(const std::vector<uint8_t>& data){
  VirusRework::validateMidi(data);
  if(privateUSB){privateUSB->sendMessage(data);return;}
  std::array<uint8_t,4096> storage{};auto* list=(MIDIPacketList*)storage.data();auto* p=MIDIPacketListInit(list);if(!MIDIPacketListAdd(list,sizeof(storage),p,0,data.size(),data.data()))throw std::runtime_error("MIDI packet too large");check(MIDISend(output,destination,list),"MIDI send");
 }
 std::vector<std::vector<uint8_t>> received(){std::lock_guard<std::mutex> guard(lock);return messages;}
 std::string origin(size_t index){std::lock_guard<std::mutex> guard(lock);return origins.at(index);}
 std::vector<uint8_t> single(uint8_t part,const std::function<void()>& idle={}){auto before=received().size();request(0x30,part);for(int n=0;n<100;n++){if(idle)idle();std::this_thread::sleep_for(std::chrono::milliseconds(10));auto all=received();for(size_t i=before;i<all.size();i++){auto& m=all[i];if(m.size()!=524||m[6]!=0x10||m[7]!=0||m[8]!=part)continue;unsigned a=0,b=0;for(size_t j=5;j<265;j++)a+=m[j];for(size_t j=5;j<522;j++)b+=m[j];if((a&127)!=m[265]||(b&127)!=m[522])throw std::runtime_error("Single checksum failed");return m;}}throw std::runtime_error("Single request timeout");}
 std::vector<uint8_t> multi(){auto before=received().size();request(0x31,0);for(int n=0;n<100;n++){std::this_thread::sleep_for(std::chrono::milliseconds(10));auto all=received();for(size_t i=before;i<all.size();i++){auto& m=all[i];if(m.size()!=267||m[6]!=0x11||m[7]!=0||m[8]!=127)continue;unsigned sum=0;for(int j=5;j<265;j++)sum+=m[j];if((sum&127)!=m[265])throw std::runtime_error("Multi checksum failed");return m;}}throw std::runtime_error("Multi request timeout");}
};
}
