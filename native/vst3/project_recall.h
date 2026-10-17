#pragma once
#include "project_patches.h"
#include "recall_policy.h"

namespace VirusRework {
// Caller suppresses performance events for the complete transaction. All parts
// are preflighted before the first device command; inactive state slots stay
// empty, retaining the existing sixteen-slot serialized project format.
template<class Device,class Pause,class Load>
ProjectPatches recallProject(Device& midi,const ProjectPatches& source,DeviceModel model,Pause pause,Load load){
 auto desired=normalizeProject(source,model);
 for(unsigned p=0;p<partCount(model);++p){midi.send({uint8_t(0xb0|p),64,0});midi.send({uint8_t(0xb0|p),123,0});midi.send({uint8_t(0xb0|p),120,0});}
 if(!desired.multi.empty()){
  midi.send(desired.multi);pause(multiSettleMs);
  auto actual=midi.multi();
  if(!sameMulti(actual,desired.multi,model))throw std::runtime_error("Project multi sound/routing mismatch");
  desired.multi=std::move(actual);
 }
 for(unsigned p=0;p<partCount(model);++p)if(!desired.parts[p].empty())desired.parts[p]=load(desired.parts[p],p);
 return desired;
}
}
