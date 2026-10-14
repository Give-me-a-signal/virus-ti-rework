#pragma once
#include "browser_bank.hpp"
#include <array>
namespace VirusRework {
using PatchBytes=std::vector<uint8_t>;
struct ProjectPatches {
 std::array<PatchBytes,16> parts;
 PatchBytes multi;
 bool empty()const{for(auto& p:parts)if(!p.empty())return false;return true;}
};
inline void validateMulti(const PatchBytes& p){
 if(p.size()!=267||p[0]!=0xf0||p[1]!=0||p[2]!=0x20||p[3]!=0x33||p[4]!=1||p[6]!=0x11||p[7]!=0||p[8]!=127||p[266]!=0xf7)throw std::runtime_error("Invalid RAM multi dump");
 unsigned sum=0;for(size_t i=1;i<266;++i){if(p[i]>=128)throw std::runtime_error("Invalid multi data");if(i>=5&&i<265)sum+=p[i];}if((sum&127)!=p[265])throw std::runtime_error("Multi checksum failed");
}
inline void multiChecksum(PatchBytes& p){unsigned sum=0;for(size_t i=5;i<265;++i)sum+=p[i];p[265]=sum&127;}
inline ProjectPatches normalizeProject(const ProjectPatches& source){auto result=source;for(unsigned p=0;p<16;++p)if(!result.parts[p].empty())result.parts[p]=virus::patches::ram_single(virus::patches::parse_single(result.parts[p]),p);if(!result.multi.empty())validateMulti(result.multi);return result;}
}
