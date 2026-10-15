#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace virus::identity {
enum class Family { unknown, ti, snow };
enum class MidiPort { none, plugin, external };
inline constexpr uint16_t vendor=0x133e, validatedProduct=0x0815, otherLegacyProduct=0x0816;
// Exact names only, ASCII case-insensitive. TI/TI2 form-factor aliases describe
// discovery, not hardware certification. See docs/device-model-audit.md.
inline constexpr std::array<std::string_view,9> tiNames{{
 "Virus TI", "Virus TI Desktop", "Virus TI Keyboard", "Virus TI Polar",
 "Virus TI2", "Virus TI2 Desktop", "Virus TI2 Keyboard", "Virus TI2 Polar",
 "Virus TI Snow"
}};
inline bool equal(std::string_view a,std::string_view b) noexcept {
 if(a.size()!=b.size())return false;
 auto lower=[](unsigned char c){return c>='A'&&c<='Z'?c+('a'-'A'):c;};
 for(size_t i=0;i<a.size();++i)if(lower(a[i])!=lower(b[i]))return false;
 return true;
}
inline Family family(std::string_view name) noexcept {
 for(auto known:tiNames)if(equal(name,known))return equal(known,"Virus TI Snow")?Family::snow:Family::ti;
 return Family::unknown;
}
inline MidiPort midiPort(std::string_view name) noexcept {
 for(auto role:{MidiPort::plugin,MidiPort::external}){
  auto suffix=role==MidiPort::plugin?std::string_view(" USB Plugin I/O"):std::string_view(" USB External I/O");
  if(name.size()>suffix.size()&&equal(name.substr(name.size()-suffix.size()),suffix)&&
     family(name.substr(0,name.size()-suffix.size()))!=Family::unknown)return role;
 }
 return MidiPort::none;
}
inline bool legacyUSB(unsigned vid,unsigned pid) noexcept {
 return vid==vendor&&(pid==validatedProduct||pid==otherLegacyProduct);
}
inline void requireTransportProduct(unsigned pid){
 if(pid==otherLegacyProduct)throw std::runtime_error("Virus TI USB product 0x0816 detected; transport unverified, connection disabled. Please provide model/USB diagnostics (issue #8).");
 if(pid!=validatedProduct)throw std::runtime_error("Unrecognized Virus USB product; connection disabled");
}
struct AudioCandidate {uint32_t id;Family model;};
inline uint32_t selectAudioDevice(const std::vector<AudioCandidate>& devices){
 AudioCandidate found{0,Family::unknown};
 for(auto d:devices)if(d.model!=Family::unknown){
  if(found.id)throw std::runtime_error("Multiple Virus TI devices; connect only one for this test");
  if(!d.id)throw std::runtime_error("Invalid Virus TI audio device identity");
  found=d;
 }
 if(!found.id)throw std::runtime_error("Virus TI USB audio absent");
 if(found.model==Family::snow)throw std::runtime_error("Virus TI Snow detected; Snow support is unverified and connection is disabled. This build uses the 16-part TI transport; Snow needs model-specific MIDI/USB and recovery validation (issue #8).");
 return found.id;
}
}
