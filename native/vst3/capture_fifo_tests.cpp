#include "capture_fifo.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>
int main(){
 // A suspended host must discard a full emulator buffer instead of replaying stale voices.
 {
  auto fifo=std::make_unique<VirusRework::CaptureFIFO>();fifo->setup(48000);fifo->setTarget(8192);
  fifo->beginBlock(false);assert(fifo->latency(48000)==8192);
  std::vector<float> stale(32768*6,.5f);fifo->push(stale.data(),32768);fifo->beginBlock(true);
  std::array<int32_t,6> value{};assert(!fifo->pop(value));
  std::vector<float> fresh(8448*6,.125f);fifo->push(fresh.data(),8448);fifo->beginBlock(true);
  assert(fifo->pop(value));assert(std::abs(value[0]/2147483648.-.125)<1e-8);
 }
 for(double rate:{44100.,48000.,96000.})for(double drift:{-.0003,.0003}){
  auto fifo=std::make_unique<VirusRework::CaptureFIFO>();fifo->setup(rate);
  uint64_t produced=0,output=0,valid=0;double energy=0;unsigned crossings=0;double previous=0;
  std::array<float,512*6> input{};
  for(unsigned block=0;block<unsigned(rate*20/128);block++){
   const double time=double(output)/rate;
   while(double(produced)/(48000*(1+drift))<=time){
    for(unsigned f=0;f<512;f++)for(unsigned c=0;c<6;c++)input[f*6+c]=float(.02*(c+1)*std::sin(2*3.141592653589793*440*(produced+f)/48000));
    fifo->push(input.data(),512);produced+=512;
   }
   fifo->beginBlock(true);
   for(unsigned f=0;f<128;f++){std::array<int32_t,6> value{};if(fifo->pop(value)&&output>uint64_t(rate)){
    double x=value[0]/2147483648.;energy+=x*x;valid++;if(previous<0&&x>=0)crossings++;previous=x;
    // Plug-in channels1/2 correspond to raw channels3/4.
    assert(std::abs(value[1]/2147483648.-x*4/3)<2e-7);
    assert(std::abs(value[4]/2147483648.-x/3)<2e-7);
   }output++;}
  }
  assert(fifo->overruns==0&&fifo->underruns==0);
  const double hz=crossings*rate/valid;
  assert(hz>439&&hz<441);
  assert(std::abs(std::sqrt(energy/valid)-.06/std::sqrt(2.))<.0001);
  fifo->beginBlock(false);std::array<int32_t,6> empty{};assert(!fifo->pop(empty));
  std::cout<<rate<<" Hz, drift="<<drift<<", measured="<<hz<<", zero queue errors\n";
 }
}
