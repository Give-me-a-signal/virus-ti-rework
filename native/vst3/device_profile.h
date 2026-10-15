#pragma once
#include <stdexcept>
#include <string_view>
#include <initializer_list>
#include "../device-identity/include/virus/device_identity.hpp"

namespace VirusRework {
enum class DeviceModel { ti16, snow4 };
inline DeviceModel connectionProfile(virus::identity::Family family,bool snowOnly=false){
 if(family==virus::identity::Family::snow)return DeviceModel::snow4;
 if(family==virus::identity::Family::ti){
  if(snowOnly)throw std::runtime_error("Snow test requires a Snow device");
  return DeviceModel::ti16;
 }
 throw std::runtime_error("Unknown device profile");
}
#ifdef VTR_SNOW_PLAYBACK_TEST_BUILD
inline constexpr bool snowPlaybackTestBuild=true;
#else
inline constexpr bool snowPlaybackTestBuild=false;
#endif
inline constexpr std::string_view snowConsentRecord="VTR-SNOW-PLAYBACK-TEST-1\n";
inline unsigned partCount(DeviceModel model){
 switch(model){case DeviceModel::ti16:return 16;case DeviceModel::snow4:return 4;}
 throw std::runtime_error("Unknown device profile");
}
inline void requirePart(DeviceModel model,unsigned part){
 if(part>=partCount(model))throw std::runtime_error("Part outside connected device profile");
}
inline std::string_view profileRecord(DeviceModel model){
 switch(model){case DeviceModel::ti16:return "VTR-PROFILE-1 ti16\n";case DeviceModel::snow4:return "VTR-PROFILE-1 snow4\n";}
 throw std::runtime_error("Unknown device profile");
}
inline DeviceModel parseProfile(std::string_view record){
 for(auto model:{DeviceModel::ti16,DeviceModel::snow4})if(record==profileRecord(model))return model;
 throw std::runtime_error("Invalid recovery device profile");
}
// Connect TI selects the profile explicitly for this session. Recovery must
// retain that session record; build branding is not a transport permission.
// Authorization does not establish physical Snow compatibility.
inline void requireSessionProfile(DeviceModel model,bool snowSession=false){
 if(model==DeviceModel::snow4&&!snowSession)throw std::runtime_error("Snow recovery requires the connection session record; physical support remains unverified (issue #8)");
 partCount(model);
}
}
