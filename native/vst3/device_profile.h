#pragma once
#include <stdexcept>
#include <string_view>
#include <initializer_list>

namespace VirusRework {
enum class DeviceModel { ti16, snow4 };
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
// The four-part algorithms are testable offline, but do not establish a Snow
// initialization/USB/mode contract. Both the service and helper enforce this.
inline void requireVerifiedProfile(DeviceModel model){
 if(model==DeviceModel::snow4)throw std::runtime_error("Snow four-part support is unverified; hardware connection and recovery are disabled pending model-specific validation (issue #8)");
 partCount(model);
}
}
