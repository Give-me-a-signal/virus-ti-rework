#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
using namespace Steinberg;
using namespace Steinberg::Vst;
static std::atomic<bool> stopBench{false};
// Loads the actual bundle, never the statically linked processor. Hardware is
// connected only by a separate explicit local-API command. A trigger file starts
// a finite, sample-offset MIDI fixture after the operator verifies readiness.
int main(int argc,char** argv){@autoreleasepool{try{
 if(argc!=8)throw std::runtime_error("usage: protocol_bench vst3|au BUNDLE OUTPUT.f32 TRIGGER RATE BLOCK SECONDS");
 const bool au=std::string(argv[1])=="au";if(!au&&std::string(argv[1])!="vst3")throw std::runtime_error("Invalid format");
 const double rate=std::stod(argv[5]);unsigned block=std::stoul(argv[6]),seconds=std::stoul(argv[7]);
 if((rate!=44100&&rate!=48000&&rate!=96000)||block<16||block>4096||seconds<1||seconds>1800)throw std::runtime_error("Invalid bench limits");
 [NSApplication sharedApplication];std::signal(SIGTERM,[](int){stopBench=true;});std::signal(SIGINT,[](int){stopBench=true;});
 std::ofstream output(argv[3],std::ios::binary|std::ios::out);if(!output)throw std::runtime_error("Cannot open recording");
 std::array<std::vector<float>,6> channels;for(auto& c:channels)c.resize(block);std::array<float*,6> pointers{};for(unsigned c=0;c<6;++c)pointers[c]=channels[c].data();
 std::vector<float> interleaved(block*6);std::array<AudioBusBuffers,3> buses{};for(unsigned b=0;b<3;++b){buses[b].numChannels=2;buses[b].channelBuffers32=pointers.data()+2*b;}
 VST3::Hosting::Module::Ptr module;IPtr<HostApplication> application;IPtr<PlugProvider> provider;IPtr<IComponent> component;IPtr<IAudioProcessor> processor;
 CFBundleRef bundle=nullptr;AudioUnit unit=nullptr;
 if(au){
  NSURL* url=[NSURL fileURLWithPath:@(argv[2])];bundle=CFBundleCreate(nullptr,(__bridge CFURLRef)url);if(!bundle||!CFBundleLoadExecutable(bundle))throw std::runtime_error("Cannot load AU bundle");
  auto factory=(AudioComponentFactoryFunction)CFBundleGetFunctionPointerForName(bundle,CFSTR("AUWrapperFactory"));if(!factory)throw std::runtime_error("No AU factory");
  AudioComponentDescription desc{'aumu','VtRw','GmAS',0,0};auto registered=AudioComponentRegister(&desc,CFSTR("Virus TI Rework bench"),8194,factory);
  if(!registered||AudioComponentInstanceNew(registered,&unit))throw std::runtime_error("AU creation failed");
  for(unsigned b=0;b<3;++b){AudioStreamBasicDescription fmt{rate,kAudioFormatLinearPCM,kAudioFormatFlagIsFloat|kAudioFormatFlagIsNonInterleaved|kAudioFormatFlagIsPacked,4,1,4,2,32,0};if(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,b,&fmt,sizeof fmt))throw std::runtime_error("AU format failed");}
  UInt32 max=block;if(AudioUnitSetProperty(unit,kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,0,&max,sizeof max)||AudioUnitInitialize(unit))throw std::runtime_error("AU activation failed");
 }else{
  application=owned(new HostApplication);PluginContextFactory::instance().setPluginContext(application);
  std::string error;module=VST3::Hosting::Module::create(argv[2],error);if(!module)throw std::runtime_error(error);
  auto factory=module->getFactory();factory.setHostContext(application);
  for(const auto& info:factory.classInfos())if(info.category()==kVstAudioEffectClass&&info.name()=="Virus TI Rework"){provider=owned(new PlugProvider(factory,info,true));if(!provider->initialize())throw std::runtime_error("VST3 initialization failed");break;}
  if(!provider)throw std::runtime_error("Expected VST3 class absent");component=provider->getComponentPtr();FUnknownPtr<IAudioProcessor> queried(component);processor=queried;if(!processor)throw std::runtime_error("No processor");
  SpeakerArrangement arrangements[3]{SpeakerArr::kStereo,SpeakerArr::kStereo,SpeakerArr::kStereo};if(processor->setBusArrangements(nullptr,0,arrangements,3)!=kResultOk)throw std::runtime_error("VST3 bus setup failed");
  for(unsigned b=0;b<3;++b)component->activateBus(kAudio,kOutput,b,true);component->activateBus(kEvent,kInput,0,true);
  ProcessSetup setup{kRealtime,kSample32,int32(block),rate};if(processor->setupProcessing(setup)!=kResultOk||component->setActive(true)!=kResultOk||processor->setProcessing(true)!=kResultOk)throw std::runtime_error("VST3 activation failed");
 }
 EventList events(32);ProcessContext context{};context.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|ProcessContext::kPlaying;context.sampleRate=rate;context.tempo=120;
 uint64_t frame=0,fixtureStart=0;bool triggered=false;unsigned sentOn=0,sentOff=0;auto next=std::chrono::steady_clock::now();auto end=next+std::chrono::seconds(seconds);
 std::cout<<"BENCH_READY "<<(au?"AU":"VST3")<<" "<<rate<<"/"<<block<<"; hardware remains disconnected until explicit Connect\n"<<std::flush;
 while(!stopBench&&std::chrono::steady_clock::now()<end){
  if(!triggered&&std::filesystem::exists(argv[4])){triggered=true;fixtureStart=frame+uint64_t(rate*2);std::cout<<"FIXTURE_START "<<fixtureStart<<"\n"<<std::flush;}
  events.clear();
  if(triggered)for(unsigned note=0;note<256;++note)for(unsigned off=0;off<2;++off){uint64_t at=fixtureStart+uint64_t(std::llround((note*.125+off*.060)*rate));if(at<frame||at>=frame+block)continue;
   if(off)++sentOff;else ++sentOn;unsigned pitch=60+(note%4);
   if(au){if(MusicDeviceMIDIEvent(unit,off?0x80:0x90,pitch,off?0:30,UInt32(at-frame)))throw std::runtime_error("AU MIDI failed");}
   else{Event e{};e.busIndex=0;e.sampleOffset=int32(at-frame);if(off){e.type=Event::kNoteOffEvent;e.noteOff.channel=0;e.noteOff.pitch=pitch;e.noteOff.velocity=0;e.noteOff.noteId=note;}else{e.type=Event::kNoteOnEvent;e.noteOn.channel=0;e.noteOn.pitch=pitch;e.noteOn.velocity=30.f/127;e.noteOn.noteId=note;}events.addEvent(e);}
  }
  if(au){for(unsigned b=0;b<3;++b){struct{UInt32 n;AudioBuffer b[2];} list{2,{{1,UInt32(block*4),pointers[2*b]},{1,UInt32(block*4),pointers[2*b+1]}}};AudioTimeStamp t{};t.mFlags=kAudioTimeStampSampleTimeValid;t.mSampleTime=frame;AudioUnitRenderActionFlags flags=0;if(AudioUnitRender(unit,&flags,&t,b,block,(AudioBufferList*)&list))throw std::runtime_error("AU render failed");}}
  else{context.projectTimeSamples=frame;context.projectTimeMusic=frame/rate*2;ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=block;data.numOutputs=3;data.outputs=buses.data();data.inputEvents=&events;data.processContext=&context;if(processor->process(data)!=kResultOk)throw std::runtime_error("VST3 render failed");}
  for(unsigned f=0;f<block;++f)for(unsigned c=0;c<6;++c)interleaved[f*6+c]=channels[c][f];output.write((const char*)interleaved.data(),interleaved.size()*4);frame+=block;
  next+=std::chrono::nanoseconds(int64_t(std::llround(block*1e9/rate)));std::this_thread::sleep_until(next);
 }
 if(au){AudioUnitUninitialize(unit);AudioComponentInstanceDispose(unit);CFRelease(bundle);}else{processor->setProcessing(false);component->setActive(false);processor=nullptr;component=nullptr;provider=nullptr;module->getFactory().setHostContext(nullptr);module.reset();PluginContextFactory::instance().setPluginContext(nullptr);application=nullptr;}
 std::cout<<"BENCH_COMPLETE frames="<<frame<<" note_on="<<sentOn<<" note_off="<<sentOff<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}}
