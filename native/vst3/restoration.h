#pragma once
#include "project_patches.h"
#include <fstream>
#include <functional>
#include <filesystem>

namespace VirusRework {
struct RestoreSnapshot {
 DeviceModel model=DeviceModel::ti16;
 ProjectPatches project;
 PatchBytes single;
 void validate()const{
  normalizeProject(project,model);
  for(unsigned p=0;p<partCount(model);++p){const auto& bytes=project.parts[p];virus::patches::parse_single(bytes);if(bytes.size()!=524||bytes[7]!=0||bytes[8]!=p)throw std::runtime_error("Restoration part identity mismatch");}
  virus::patches::parse_single(single);if(single.size()!=524||single[7]!=0||single[8]!=64)throw std::runtime_error("Restoration Single identity mismatch");
  validateMulti(project.multi);
 }
 static RestoreSnapshot read(const std::string& directory){
  auto read=[&](const std::string& name,size_t size){
   std::ifstream f(directory+"/"+name,std::ios::binary);PatchBytes bytes(size);
   if(!f.read(reinterpret_cast<char*>(bytes.data()),size)||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Invalid restoration file: "+name);
   return bytes;
  };
  RestoreSnapshot result;
  const auto profile=std::filesystem::path(directory)/"before-device-profile.txt";
  if(std::filesystem::exists(profile)){
   if(!std::filesystem::is_regular_file(profile)||std::filesystem::file_size(profile)>64)throw std::runtime_error("Invalid recovery device profile file");
   std::ifstream file(profile);if(!file)throw std::runtime_error("Cannot read recovery device profile");
   result.model=parseProfile(std::string(std::istreambuf_iterator<char>(file),{}));
  } // Existing recovery directories without a marker retain the TI16 contract.
  for(unsigned p=partCount(result.model);p<16;++p)if(std::filesystem::exists(std::filesystem::path(directory)/("before-part"+std::to_string(p)+".syx")))throw std::runtime_error("Unexpected inactive restoration part");
  for(unsigned p=0;p<partCount(result.model);++p){auto& bytes=result.project.parts[p];bytes=read("before-part"+std::to_string(p)+".syx",524);virus::patches::parse_single(bytes);if(bytes[7]!=0||bytes[8]!=p)throw std::runtime_error("Restoration part identity mismatch");}
  result.single=read("before-single.syx",524);virus::patches::parse_single(result.single);if(result.single[7]!=0||result.single[8]!=64)throw std::runtime_error("Restoration Single identity mismatch");
  result.project.multi=read("before-multi.syx",267);result.validate();return result;
 }
};

template<class Device,class Save,class Check>
RestoreSnapshot captureSnapshot(Device& midi,DeviceModel model,Save save,Check check){
 RestoreSnapshot result;result.model=model;partCount(model);
 check();result.single=midi.single(64);check();result.project.multi=midi.multi();
 for(unsigned p=0;p<partCount(model);++p){check();result.project.parts[p]=midi.single(p);}
 result.validate(); // No snapshot is publishable until every original is valid.
 const auto record=profileRecord(model);save("before-device-profile.txt",PatchBytes(record.begin(),record.end()));
 save("before-single.syx",result.single);save("before-multi.syx",result.project.multi);
 for(unsigned p=0;p<partCount(model);++p)save("before-part"+std::to_string(p)+".syx",result.project.parts[p]);
 return result;
}

// Same RAM-only restoration sequence as explicit Disconnect. The caller owns
// the device lease throughout writes and readback. Keep going after one failed
// operation so an error on one part does not abandon the remaining parts.
template<class Device,class Save,class Pause>
void restoreSnapshot(Device& midi,const RestoreSnapshot& original,Save save,Pause pause,bool normalizeUSB=true){
 original.validate(); // Refuse corrupt/incomplete snapshots before any command.
 const auto count=partCount(original.model);
 std::string errors;
 auto attempt=[&](const std::string& label,auto action){try{action();}catch(const std::exception& e){errors+=label+": "+e.what()+"; ";}};
 for(unsigned p=0;p<count;++p)attempt("Notes off",[&]{midi.send({uint8_t(0xb0|p),64,0});midi.send({uint8_t(0xb0|p),123,0});midi.send({uint8_t(0xb0|p),120,0});});
 if(normalizeUSB)attempt("USB normalization",[&]{midi.send({0xf0,0,0x20,0x33,1,16,0x73,0,9,0,0xf7});});
 attempt("Single mode",[&]{midi.send({0xf0,0,0x20,0x33,1,16,0x73,0,122,0,0xf7});});pause(350);
 const auto& multi=original.project.multi;
 attempt("Multi restore",[&]{midi.send(multi);});
 pause(250);
 // These serialized reference offsets are proven only on our TI Desktop.
 // Snow's opaque multi + four sound buffers are preserved without guessing
 // part-reference fields. Its physical recovery guard remains in place.
 if(original.model==DeviceModel::ti16)for(unsigned p=0;p<count;++p)attempt("Part references",[&]{midi.send({0xf0,0,0x20,0x33,1,16,0x72,uint8_t(p),32,multi[41+p],0xf7});midi.send({0xf0,0,0x20,0x33,1,16,0x72,uint8_t(p),33,multi[57+p],0xf7});});
 pause(300);
 for(unsigned p=0;p<count;++p){attempt("Part write",[&]{midi.send(original.project.parts[p]);});pause(100);}
 for(unsigned p=0;p<count;++p)attempt("Part "+std::to_string(p+1),[&]{
  auto actual=midi.single(p);save("restored-part"+std::to_string(p)+".syx",actual);auto parsed=virus::patches::parse_single(actual);const auto& before=original.project.parts[p];
  if(original.model==DeviceModel::snow4){if(actual!=before)throw std::runtime_error("Snow part sound mismatch");}
  else if(before[9]<12){if(parsed.name!=virus::patches::parse_single(before).name||midi.single(p)!=actual)throw std::runtime_error("Legacy conversion unstable");}
  else{if(actual.size()!=before.size())throw std::runtime_error("Size mismatch");for(size_t i=0;i<actual.size();++i)if(i!=10&&i!=11&&i!=12&&i!=265&&i!=522&&actual[i]!=before[i])throw std::runtime_error("Sound mismatch");}
 });
 attempt("Single buffer",[&]{auto actual=midi.single(64);if(actual!=original.single){midi.send(original.single);pause(100);actual=midi.single(64);}save("restored-single.syx",actual);if(actual!=original.single)throw std::runtime_error("Single restoration mismatch");});
 attempt("Multi readback",[&]{auto actual=midi.multi();save("restored-multi.syx",actual);if(!sameMulti(actual,multi,original.model))throw std::runtime_error("Sound/routing mismatch");});
 if(!errors.empty())throw std::runtime_error(errors);
}
}
