#pragma once
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
namespace VirusRework::Graphs {
// Analytic UI illustrations, not Virus spectral waveforms or an audio model.
inline const auto waveformBits=[] {std::array<std::array<uint32_t,64>,64> waves{};
 for(unsigned w=0;w<64;++w)for(unsigned i=0;i<64;++i){double x=i/64.;float v=w%4==0?std::sin(6.283185307179586*x):w%4==1?1-4*std::abs(x-.5):w%4==2?2*x-1:(x<.5?1:-1);std::memcpy(&waves[w][i],&v,4);}return waves;}();
}
