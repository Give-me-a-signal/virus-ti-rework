#include "virus/device_identity.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace virus::identity;
template<class F> void rejects(F action,std::string_view text){
 bool rejected=false;
 try{action();}catch(const std::runtime_error& e){rejected=true;assert(std::string_view(e.what()).find(text)!=std::string_view::npos);}
 assert(rejected);
}
int main(){
 // Every TI/TI2 form-factor label, plus the generic USB name used across models.
 for(auto name:tiNames){
  const auto expected=name=="Virus TI Snow"?Family::snow:Family::ti;
  assert(family(name)==expected);
  std::string upper(name);for(auto& c:upper)if(c>='a'&&c<='z')c-=32;
  assert(family(upper)==expected);
  assert(midiPort(std::string(name)+" USB Plugin I/O")==MidiPort::plugin);
  assert(midiPort(std::string(name)+" USB External I/O")==MidiPort::external);
  assert(midiPort(upper+" USB PLUGIN I/O")==MidiPort::plugin);
  assert(midiPort(std::string(name)+" USB Plugin I/O extra")==MidiPort::none);
  assert(family(std::string(name)+" extra")==Family::unknown);
  if(expected==Family::ti)assert(selectAudioDevice({{19,expected}})==19);
 }
 // Snow was formerly incorrectly reported absent. It is now recognized, but
 // must never escape selection into the sixteen-part snapshot/write path.
 rejects([]{selectAudioDevice({{42,family("Virus TI Snow")}});},"Snow detected");
 rejects([]{selectAudioDevice({{42,Family::snow}});},"unverified");
 rejects([]{selectAudioDevice({{42,Family::snow}});},"connection is disabled");
 rejects([]{selectAudioDevice({{1,Family::ti},{2,Family::snow}});},"Multiple");
 rejects([]{selectAudioDevice({{2,Family::snow},{1,Family::ti}});},"Multiple");
 rejects([]{selectAudioDevice({{1,Family::ti},{2,Family::ti}});},"Multiple");
 rejects([]{selectAudioDevice({{1,Family::snow},{2,Family::snow}});},"Multiple");
 rejects([]{selectAudioDevice({});},"audio absent");
 rejects([]{selectAudioDevice({{4,Family::unknown}});},"audio absent");
 rejects([]{selectAudioDevice({{0,Family::ti}});},"Invalid");
 assert(selectAudioDevice({{7,Family::unknown},{8,Family::ti}})==8);
 for(auto name:{"", "Virus", "Virus A", "Virus B", "Virus C", "Virus Classic", "Virus Indigo", "Virus Rack", "Virus PowerCore", "Virus TI2X", "Virus TI Snowball", "Not Virus TI", "Virus TI USB", " Virus TI", "Virus TI "}){
  assert(family(name)==Family::unknown);assert(midiPort(name)==MidiPort::none);
 }
 for(auto name:{"USB Plugin I/O", "Other USB Plugin I/O", "Virus TI Snowball USB Plugin I/O", "Virus TI USB MIDI", "Virus TI USB External I/O extra"})assert(midiPort(name)==MidiPort::none);
 assert(legacyUSB(0x133e,0x0815));assert(legacyUSB(0x133e,0x0816));
 assert(!legacyUSB(0x133f,0x0815));assert(!legacyUSB(0x133e,0x0817));
 assert(!legacyUSB(0x1133e,0x0815));assert(!legacyUSB(0x133e,0x10815));
 requireTransportProduct(0x0815);
 rejects([]{requireTransportProduct(0x0816);},"0x0816 detected");
 rejects([]{requireTransportProduct(0x0816);},"unverified");
 rejects([]{requireTransportProduct(0x9999);},"Unrecognized");
 std::cout<<"TI/TI2 model and MIDI aliases, Snow preflight, ambiguity and USB identity tests passed (no hardware I/O)\n";
}
