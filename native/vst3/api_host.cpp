#include "plugin.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <cmath>
using namespace VirusRework;
std::atomic<bool> stopping{false};
int main(int argc,char** argv){
 if(argc<2||argc>4){std::cerr<<"usage: vst3_api_host OUTPUT-six-channel.f32 [RATE [BLOCK]]\n";return 1;}
 double sampleRate=48000;unsigned blockSize=256;
 try{size_t used=0;if(argc>2){sampleRate=std::stod(argv[2],&used);if(used!=std::string(argv[2]).size())throw 0;}if(argc>3){blockSize=std::stoul(argv[3],&used);if(used!=std::string(argv[3]).size())throw 0;}}catch(...){std::cerr<<"Invalid rate/block\n";return 1;}
 if(!std::isfinite(sampleRate)||sampleRate<32000||sampleRate>192000||blockSize<16||blockSize>4096){std::cerr<<"Rate/block out of range\n";return 1;}
 std::signal(SIGINT,[](int){stopping=true;});std::signal(SIGTERM,[](int){stopping=true;});
 Processor processor;processor.initialize(nullptr);ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;setup.sampleRate=sampleRate;setup.maxSamplesPerBlock=blockSize;
 processor.setupProcessing(setup);processor.setActive(true);processor.setProcessing(true);
 std::array<std::vector<float>,6> samples;for(auto& channel:samples)channel.resize(blockSize);std::array<float*,6> pointers;for(unsigned c=0;c<6;++c)pointers[c]=samples[c].data();
 std::array<AudioBusBuffers,3> buses{};for(unsigned b=0;b<3;++b){buses[b].numChannels=2;buses[b].channelBuffers32=&pointers[2*b];}
 ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=blockSize;data.numOutputs=3;data.outputs=buses.data();
 std::ofstream audio(argv[1],std::ios::binary);std::vector<float> interleaved(blockSize*6);
 std::cout<<"API_HOST_READY realtime "<<sampleRate<<"Hz, block "<<blockSize<<", 3 stereo returns; discover with ti_control.py list"<<std::endl;
 auto next=std::chrono::steady_clock::now();auto end=next+std::chrono::minutes(30);
 while(!stopping&&std::chrono::steady_clock::now()<end){processor.process(data);for(unsigned f=0;f<blockSize;++f)for(unsigned c=0;c<6;++c)interleaved[f*6+c]=samples[c][f];audio.write(reinterpret_cast<const char*>(interleaved.data()),interleaved.size()*sizeof(float));next+=std::chrono::nanoseconds(int64_t(std::llround(blockSize*1e9/sampleRate)));std::this_thread::sleep_until(next);}
 processor.setProcessing(false);processor.setActive(false);processor.terminate();
}
