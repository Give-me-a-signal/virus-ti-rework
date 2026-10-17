#pragma once
#include "browser_bank.hpp"
#include "device_profile.h"
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
inline ProjectPatches normalizeProject(const ProjectPatches& source,DeviceModel model=DeviceModel::ti16){
 const auto count=partCount(model);
 for(unsigned p=count;p<source.parts.size();++p)if(!source.parts[p].empty())throw std::runtime_error("Project contains parts outside connected device profile");
 auto result=source;for(unsigned p=0;p<count;++p)if(!result.parts[p].empty())result.parts[p]=virus::patches::ram_single(virus::patches::parse_single(result.parts[p]),p);if(!result.multi.empty())validateMulti(result.multi);return result;
}
inline bool sameMulti(const PatchBytes& actual,const PatchBytes& expected,DeviceModel model){
 validateMulti(actual);validateMulti(expected);partCount(model);
 // Existing TI flag normalization is observed on hardware. Do not silently
 // apply it to Snow: there is no Snow readback evidence for that exception.
 if(model==DeviceModel::snow4)return actual==expected;
 for(size_t i=0;i<expected.size();++i)if(i!=10&&i!=265&&actual[i]!=expected[i])return false;
 return true;
}
inline ProjectPatches normalizeSnowPlaybackProject(const ProjectPatches& source){
 auto result=normalizeProject(source,DeviceModel::snow4);
 for(unsigned p=0;p<4;++p){const auto& patch=result.parts[p];if(!patch.empty()&&(patch.size()!=524||patch[9]<12))throw std::runtime_error("Legacy Snow project patch blocked before write");}
 if(!result.multi.empty())for(unsigned p=0;p<4;++p)if(result.multi[185+p]!=10&&result.multi[185+p]!=13)throw std::runtime_error("Snow project must use USB1/2 stereo routing");
 return result;
}
}
