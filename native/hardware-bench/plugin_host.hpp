#pragma once
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "virus/vst_bridge_interface.hpp"
#include "virus/device_bridge.hpp"
#include <memory>
#include <algorithm>
#include <cmath>
using namespace Steinberg;
using namespace Steinberg::Vst;

struct BenchHost {
 virus::bridge::DeviceBridge bridge; // Must outlive the plug-in.
 VST3::Hosting::Module::Ptr module;
 IPtr<HostApplication> application;
 IPtr<PlugProvider> provider;
 IPtr<IComponent> component;
 IPtr<IAudioProcessor> processor;
 EventList events{32};
 ParameterChanges changes{1};
 std::array<std::array<float,2048>,6> channels{};
 std::array<float*,6> pointers{};
 std::array<AudioBusBuffers,3> buses{};
 std::vector<float> raw,rendered;
 std::atomic<uint64_t> frames{0};
 uint64_t callbacks=0,errors=0,publishFailures=0,timestampGaps=0;
 double expectedTimestamp=0;bool haveTimestamp=false,notes=false,automate=false;
 static constexpr uint64_t maxFrames=48000*8;
 BenchHost(bool playNotes,bool testCC=false):notes(playNotes),automate(testCC) {
  raw.resize(maxFrames*6);rendered.resize(maxFrames*6);
  application=owned(new HostApplication);PluginContextFactory::instance().setPluginContext(application);
  std::string error;module=VST3::Hosting::Module::create(VTR_PLUGIN_PATH,error);
  if(!module)throw std::runtime_error(error);
  auto factory=module->getFactory();factory.setHostContext(application);
  for(const auto& info:factory.classInfos())if(info.category()==kVstAudioEffectClass&&info.name()=="Virus TI Rework (Development)") {provider=owned(new PlugProvider(factory,info,true));if(!provider->initialize())throw std::runtime_error("VST3 initialization failed");break;}
  if(!provider)throw std::runtime_error("Expected VST3 absent");component=provider->getComponentPtr();
  FUnknownPtr<IAudioProcessor> proc(component);processor=proc;if(!processor)throw std::runtime_error("No audio processor");
  virus::bridge::IVirusDeviceBridge* api=nullptr;
  if(component->queryInterface(virus::bridge::IVirusDeviceBridge::iid,reinterpret_cast<void**>(&api))!=kResultOk||!api)throw std::runtime_error("Bridge interface absent");
  auto attached=api->attachDeviceBridge(&bridge);api->release();if(attached!=kResultOk)throw std::runtime_error("Bridge attach failed");
  SpeakerArrangement arrangements[3]={SpeakerArr::kStereo,SpeakerArr::kStereo,SpeakerArr::kStereo};
  if(processor->setBusArrangements(nullptr,0,arrangements,3)!=kResultOk)throw std::runtime_error("Bus setup failed");
  for(int b=0;b<3;b++){if(component->activateBus(kAudio,kOutput,b,true)!=kResultOk)throw std::runtime_error("Bus activation failed");for(int c=0;c<2;c++)pointers[b*2+c]=channels[b*2+c].data();buses[b].numChannels=2;buses[b].channelBuffers32=&pointers[b*2];}
  component->activateBus(kEvent,kInput,0,true);
  ProcessSetup setup{kRealtime,kSample32,2048,48000};
  if(processor->setupProcessing(setup)!=kResultOk||component->setActive(true)!=kResultOk||processor->setProcessing(true)!=kResultOk)throw std::runtime_error("VST3 activation failed");
  // The CoreAudio capture callback is also our plug-in process clock: same
  // frame count/rate, no unrelated host clock or resampling assumption.
  if(!bridge.serviceReady(bridge.generation()))throw std::runtime_error("Bridge readiness failed");
 }
 ~BenchHost(){if(processor)processor->setProcessing(false);if(component)component->setActive(false);processor=nullptr;component=nullptr;provider=nullptr;if(module)module->getFactory().setHostContext(nullptr);module.reset();PluginContextFactory::instance().setPluginContext(nullptr);application=nullptr;}
 static OSStatus callback(AudioDeviceID,const AudioTimeStamp*,const AudioBufferList* input,const AudioTimeStamp* timestamp,AudioBufferList* output,const AudioTimeStamp*,void* context) {
  auto& h=*static_cast<BenchHost*>(context);
  if(output)for(UInt32 i=0;i<output->mNumberBuffers;i++)if(output->mBuffers[i].mData)memset(output->mBuffers[i].mData,0,output->mBuffers[i].mDataByteSize);
  ++h.callbacks;
  if(!input||input->mNumberBuffers!=1||input->mBuffers[0].mNumberChannels!=6||!input->mBuffers[0].mData){++h.errors;return noErr;}
  auto n=input->mBuffers[0].mDataByteSize/(sizeof(float)*6);auto start=h.frames.load(std::memory_order_relaxed);
  if(n>2048||start+n>maxFrames)return noErr;
  if(timestamp&&(timestamp->mFlags&kAudioTimeStampSampleTimeValid)){if(h.haveTimestamp&&std::abs(timestamp->mSampleTime-h.expectedTimestamp)>.5)++h.timestampGaps;h.expectedTimestamp=timestamp->mSampleTime+n;h.haveTimestamp=true;}
  const auto* src=static_cast<const float*>(input->mBuffers[0].mData);
  for(UInt32 f=0;f<n;f++){virus::bridge::AudioFrame frame;frame.generation=h.bridge.generation();frame.host_frame=start+f;for(int c=0;c<6;c++){float v=src[f*6+c];h.raw[(start+f)*6+c]=v;if(!std::isfinite(v)){++h.errors;v=0;}frame.pcm[(c+4)%6]=static_cast<int32_t>(std::clamp(double(v)*2147483648.0,-2147483648.0,2147483647.0));}if(!h.bridge.servicePublish(frame))++h.publishFailures;}
  h.events.clear();
  h.changes.clearQueue();
  if(h.automate&&48000>=start&&48000<start+n){int32 qi=0,pi=0;auto* q=h.changes.addParameterData(268435609,qi);if(q)q->addPoint(int32(48000-start),32.0/127.0,pi);}
  if(h.notes)for(int j=0;j<6;j++){uint64_t time=48000*(j+1);if(time>=start&&time<start+n){Event e{};e.busIndex=0;e.sampleOffset=int32(time-start);if(j%2==0){e.type=Event::kNoteOnEvent;e.noteOn.channel=0;e.noteOn.pitch=60+(j/2)*4;e.noteOn.velocity=.28f;e.noteOn.noteId=j/2;}else{e.type=Event::kNoteOffEvent;e.noteOff.channel=0;e.noteOff.pitch=60+(j/2)*4;e.noteOff.velocity=0;e.noteOff.noteId=j/2;}h.events.addEvent(e);}}
  ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=n;data.numOutputs=3;data.outputs=h.buses.data();data.inputEvents=&h.events;data.inputParameterChanges=&h.changes;
  if(h.processor->process(data)!=kResultOk)++h.errors;
  for(UInt32 f=0;f<n;f++)for(int c=0;c<6;c++)h.rendered[(start+f)*6+c]=h.channels[c][f];
  h.frames.store(start+n,std::memory_order_release);return noErr;
 }
};
static void writeWave(const std::string& path,const std::vector<float>& samples,uint64_t frames){
 std::ofstream out(path,std::ios::binary);auto u16=[&](uint16_t v){out.write((char*)&v,2);};auto u32=[&](uint32_t v){out.write((char*)&v,4);};uint32_t bytes=uint32_t(frames*6*4);
 out.write("RIFF",4);u32(36+bytes);out.write("WAVEfmt ",8);u32(16);u16(3);u16(6);u32(48000);u32(48000*24);u16(24);u16(32);out.write("data",4);u32(bytes);out.write((const char*)samples.data(),bytes);if(!out)throw std::runtime_error("WAV write failed");
}
