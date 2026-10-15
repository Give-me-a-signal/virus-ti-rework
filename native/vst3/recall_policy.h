#pragma once
#include "project_patches.h"
namespace VirusRework {
// TI Revive issue #5 / evidence E145: the synth can discard commands directly
// after a multi dump. Delay starts AFTER the last USB transfer completes.
inline constexpr unsigned multiSettleMs=250;
inline void verifyMultiRecall(const PatchBytes& wanted,const PatchBytes& actual){
 validateMulti(wanted);validateMulti(actual);
 for(size_t i=0;i<wanted.size();++i)
  if(i!=10&&i!=265&&wanted[i]!=actual[i])throw std::runtime_error("Multi sound/routing recall mismatch");
}
}
