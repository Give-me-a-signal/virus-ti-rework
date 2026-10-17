#include "plugin.h"
#include "editor.h"
#include "browser_bank.hpp"
#include "midi_core.hpp"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "base/source/fstreamer.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <limits>
namespace VirusRework {
static_assert(std::atomic<double>::is_always_lock_free, "Audio state requires lock-free atomics");
namespace {
constexpr uint32_t kMagic=0x56545257, kVersion=3;
using Snapshot=std::array<double,kCatalog.size()>;
size_t indexOf(ParamID id) {
 auto p=std::lower_bound(kCatalog.begin(),kCatalog.end(),id,[](const auto& a,ParamID b){return a.id<b;});
 return p!=kCatalog.end() && p->id==id ? static_cast<size_t>(p-kCatalog.begin()) : kCatalog.size();
}
uint32_t patchCRC(const std::vector<uint8_t>& patch){uint32_t crc=~0u;for(auto byte:patch){crc^=byte;for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int(crc&1)));}return ~crc;}
void patchControls(Snapshot& snapshot,const std::vector<uint8_t>& patch,unsigned part=0){
 if(patch.empty())return;
 for(const auto& value:virus::patches::parse_single(patch).decoded_values())for(size_t i=0;i<kCatalog.size();++i){const auto& p=kCatalog[i];if(p.part==part+1&&std::strcmp(p.name,value.name)==0)snapshot[i]=std::clamp((value.value-p.minimum)/(p.maximum-p.minimum),0.,1.);}
}
bool readState(IBStream* stream, Snapshot& snapshot,ProjectPatches& project,bool& reconnect) {
 if(!stream) return false;
 IBStreamer s(stream,kLittleEndian); uint32 magic=0,version=0,count=0;
 if(!s.readInt32u(magic)||!s.readInt32u(version)||!s.readInt32u(count)||magic!=kMagic||(version<1||version>kVersion)||count!=kCatalog.size()) return false;
 for(size_t i=0;i<snapshot.size();++i) {
  uint32 id=0; double value=0;
  if(!s.readInt32u(id)||!s.readDouble(value)||id!=kCatalog[i].id||!std::isfinite(value)||value<0||value>1) return false;
  snapshot[i]=value;
 }
 auto readPatch=[&](PatchBytes& patch,bool multi){uint32 size=0,crc=0;if(!s.readInt32u(size)||(size!=0&&size!=267&&(multi||size!=524)))return false;
  patch.resize(size);for(auto& byte:patch)if(!s.readInt8u(byte))return false;
  if(!s.readInt32u(crc)||crc!=patchCRC(patch))return false;
  try{if(!patch.empty()){if(multi)validateMulti(patch);else virus::patches::parse_single(patch);}}catch(...){return false;}return true;
 };
 if(version>=2){uint32 flags=0;if(!s.readInt32u(flags)||flags>1)return false;reconnect=flags!=0;
  if(version==2){if(!readPatch(project.parts[0],false))return false;}
  else{uint32 parts=0;if(!s.readInt32u(parts)||parts!=16)return false;for(auto& patch:project.parts)if(!readPatch(patch,false))return false;if(!readPatch(project.multi,true))return false;}
  if(reconnect&&project.empty())return false;
 }

 return true;
}
}
Processor::Processor() {
 disconnected_.claim();
 hardware_=std::make_unique<HardwareService>(disconnected_);
 setControllerClass(kControllerID);
 for(size_t i=0;i<kCatalog.size();++i) values_[i].store((kCatalog[i].initial-kCatalog[i].minimum)/(kCatalog[i].maximum-kCatalog[i].minimum));
 hardware_->stateCallbacks([this]{MemoryStream stream;if(getState(&stream)!=kResultOk)return std::vector<uint8_t>{};return std::vector<uint8_t>(stream.getData(),stream.getData()+stream.getSize());},[this](const std::vector<uint8_t>& bytes){MemoryStream stream(const_cast<uint8_t*>(bytes.data()),bytes.size());return setState(&stream)==kResultOk;});
}
Processor::~Processor() {hardware_->shutdownControl();hardware_.reset();bridge_->restart(false);bridge_->release();}
tresult PLUGIN_API Processor::terminate(){hardware_->shutdown();return AudioEffect::terminate();}
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
 if(!std::isfinite(setup.sampleRate)||setup.sampleRate<32000||setup.sampleRate>192000)return kInvalidArgument;
 auto result=AudioEffect::setupProcessing(setup);if(result==kResultOk)hardware_->setup(setup.sampleRate);return result;
}
uint32 PLUGIN_API Processor::getLatencySamples(){return bridge_==&disconnected_?hardware_->latency():0;}
tresult PLUGIN_API Processor::notify(IMessage* message){
 if(message&&std::strcmp(message->getMessageID(),"VTRStateRevision")==0){
  auto reply=owned(allocateMessage());if(reply){reply->setMessageID("VTRStateRevision");reply->getAttributes()->setInt("revision",hardware_->stateRevision());sendMessage(reply);}return kResultOk;
 }
 if(message&&std::strcmp(message->getMessageID(),"VTRHardware")==0){
  int64 operation=0,target=0;message->getAttributes()->getInt("operation",operation);message->getAttributes()->getInt("part",target);if(target<0||target>=16)return kInvalidArgument;
  if(bridge_==&disconnected_){
   if(operation==4){const void* data=nullptr;uint32 size=0;if(message->getAttributes()->getBinary("patch",data,size)==kResultOk&&data&&(size==267||size==524))hardware_->loadPreset(std::vector<uint8_t>(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+size),unsigned(target));}
   else hardware_->command(int(operation));
  }
  auto reply=owned(allocateMessage());if(reply){reply->setMessageID("VTRHardwareStatus");auto text=hardware_->status();reply->getAttributes()->setBinary("text",text.c_str(),uint32(text.size()+1));reply->getAttributes()->setInt("connected",hardware_->connected());auto meters=hardware_->meters();for(unsigned bus=0;bus<3;++bus)reply->getAttributes()->setFloat(("usbMeter"+std::to_string(bus)).c_str(),meters.usbPeak[bus]);for(unsigned part=0;part<16;++part)reply->getAttributes()->setFloat(("midiActivity"+std::to_string(part)).c_str(),meters.midiActivity[part]);for(unsigned part=0;part<16;++part){auto patch=hardware_->currentPreset(part);auto key="patch"+std::to_string(part);if(!patch.empty())reply->getAttributes()->setBinary(key.c_str(),patch.data(),uint32(patch.size()));}sendMessage(reply);}
  return kResultOk;
 }
 return AudioEffect::notify(message);
}
void Controller::hardwareCommand(int operation){auto message=owned(allocateMessage());if(message){message->setMessageID("VTRHardware");message->getAttributes()->setInt("operation",operation);sendMessage(message);}}
tresult PLUGIN_API Controller::setParamNormalized(ParamID id,ParamValue value){
 const auto previous=getParamNormalized(id);
 auto result=EditControllerEx1::setParamNormalized(id,value);
 if(result==kResultOk&&getParamNormalized(id)!=previous)++editorParameterRevision;
 return result;
}
void Controller::hardwarePreset(const std::vector<uint8_t>& patch,unsigned part){auto message=owned(allocateMessage());if(message){message->setMessageID("VTRHardware");message->getAttributes()->setInt("operation",4);message->getAttributes()->setInt("part",part);message->getAttributes()->setBinary("patch",patch.data(),uint32(patch.size()));sendMessage(message);}}
tresult PLUGIN_API Controller::notify(IMessage* message){
 if(message&&std::strcmp(message->getMessageID(),"VTRStateRevision")==0){
  int64 revision=0;if(message->getAttributes()->getInt("revision",revision)==kResultOk&&revision!=observedStateRevision){observedStateRevision=revision;setDirty(true);}return kResultOk;
 }
 if(message&&std::strcmp(message->getMessageID(),"VTRHardwareStatus")==0){const void* data=nullptr;uint32 bytes=0;if(message->getAttributes()->getBinary("text",data,bytes)==kResultOk&&data&&bytes>0&&bytes<4096&&static_cast<const char*>(data)[bytes-1]==0)hardwareText=static_cast<const char*>(data);int64 connected=0;message->getAttributes()->getInt("connected",connected);hardwareConnected=connected!=0;
  hardwareMeters={};if(hardwareConnected)for(unsigned bus=0;bus<3;++bus){double level=0;message->getAttributes()->getFloat(("usbMeter"+std::to_string(bus)).c_str(),level);if(std::isfinite(level)&&level>=0)hardwareMeters.usbPeak[bus]=float(level);}
  if(hardwareConnected)for(unsigned part=0;part<16;++part){double level=0;message->getAttributes()->getFloat(("midiActivity"+std::to_string(part)).c_str(),level);if(std::isfinite(level)&&level>=0&&level<=1)hardwareMeters.midiActivity[part]=float(level);}
  for(unsigned part=0;part<16;++part){auto key="patch"+std::to_string(part);if(message->getAttributes()->getBinary(key.c_str(),data,bytes)==kResultOk&&data&&(bytes==524||bytes==267)){PatchBytes patch(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+bytes);if(patch!=hardwarePatches.parts[part]){hardwarePatches.parts[part]=std::move(patch);++patchRevision;}}}hardwarePatch=hardwarePatches.parts[0];

  return kResultOk;}
 return EditControllerEx1::notify(message);
}
bool Processor::attachBridge(virus::bridge::DeviceBridge& bridge) noexcept {
 if(active_||processing_)return false;
 if(&bridge==bridge_)return true;
 if(!bridge.claim())return false;
 bridge_->restart(false);bridge_->release();bridge_=&bridge;return true;
}
tresult PLUGIN_API Processor::attachDeviceBridge(virus::bridge::DeviceBridge* bridge) {
 return attachBridge(bridge ? *bridge : disconnected_) ? kResultOk : kResultFalse;
}
tresult PLUGIN_API Processor::setActive(TBool value) {
 auto result=AudioEffect::setActive(value);if(result!=kResultOk)return result;
 active_=value!=0;if(!active_){processing_=false;if(hardware_->connected())hardware_->command(8);}
 hardware_->hostProcessing(active_&&processing_);
 bridge_->restart(active_&&processing_);hostFrame_=0;projectValid_=false;return result;
}
tresult PLUGIN_API Processor::setProcessing(TBool value) {
 processing_=value!=0;hardware_->hostProcessing(active_&&processing_);bridge_->restart(active_&&processing_);hostFrame_=0;projectValid_=false;return kResultOk;
}
tresult PLUGIN_API Processor::initialize(FUnknown* context) {
 auto r=AudioEffect::initialize(context);if(r!=kResultOk)return r;
 addAudioOutput(STR16("USB Return 1"),SpeakerArr::kStereo,kMain);
 addAudioOutput(STR16("USB Return 2"),SpeakerArr::kStereo,kAux,0);
 addAudioOutput(STR16("USB Return 3"),SpeakerArr::kStereo,kAux,0);
 addEventInput(STR16("TI MIDI"),16);
 return kResultOk;
}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement*,int32 numInputs,SpeakerArrangement* outputs,int32 numOutputs) {
 if(numInputs!=0||numOutputs!=3||!outputs) return kResultFalse;
 for(int i=0;i<3;++i)if(outputs[i]!=SpeakerArr::kStereo)return kResultFalse;
 return kResultOk;
}
tresult PLUGIN_API Processor::canProcessSampleSize(int32 s) {return s==kSample32||s==kSample64?kResultTrue:kResultFalse;}
tresult PLUGIN_API Processor::process(ProcessData& d) {
 if(d.numSamples<0||d.numSamples>processSetup.maxSamplesPerBlock||d.numOutputs<0||d.numOutputs>3||(d.numOutputs&&!d.outputs)||canProcessSampleSize(d.symbolicSampleSize)!=kResultTrue) return kInvalidArgument;
 bridge_->beginBlock();
 // projectTimeSamples can jump for transport loops/seeks. It is never treated as
 // monotonic USB time. A jump invalidates the current service clock agreement.
 if(d.processContext) {
  const auto project=d.processContext->projectTimeSamples;
  if(projectValid_&&project!=expectedProject_)bridge_->restart(active_&&processing_);
  projectValid_=true;
  expectedProject_=project;
  if(d.processContext->state&ProcessContext::kPlaying) {
   if(project<=std::numeric_limits<int64_t>::max()-d.numSamples)expectedProject_+=d.numSamples;
   else projectValid_=false;
  }
 } else projectValid_=false;
 if(bridge_==&disconnected_)hardware_->beginBlock(active_&&processing_&&d.processMode!=kOffline);
 // Host buffers always initialized, including inactive/disconnected/underrun.

 for(int b=0;b<d.numOutputs;++b) {
  auto& bus=d.outputs[b];
  if(bus.numChannels<0||bus.numChannels>2)return kInvalidArgument;
  bus.silenceFlags=(1ULL<<bus.numChannels)-1;
  for(int c=0;c<bus.numChannels;++c) {
   if(d.symbolicSampleSize==kSample32) {if(bus.channelBuffers32&&bus.channelBuffers32[c])std::fill_n(bus.channelBuffers32[c],d.numSamples,0.f);}
   else {if(bus.channelBuffers64&&bus.channelBuffers64[c])std::fill_n(bus.channelBuffers64[c],d.numSamples,0.);}
  }
 }
 if(active_&&processing_)for(int32 f=0;f<d.numSamples;++f) {
  std::array<std::int32_t,6> pcm{};
  if(bridge_==&disconnected_){if(!hardware_->render(pcm))continue;}else if(!bridge_->render(hostFrame_+static_cast<uint64_t>(f),pcm))continue;
  for(int b=0;b<d.numOutputs;++b)for(int c=0;c<d.outputs[b].numChannels;++c) {
   const auto sample=static_cast<double>(pcm[b*2+c])/2147483648.0;
   if(d.symbolicSampleSize==kSample32) {if(d.outputs[b].channelBuffers32&&d.outputs[b].channelBuffers32[c])d.outputs[b].channelBuffers32[c][f]=static_cast<float>(sample);}
   else if(d.outputs[b].channelBuffers64&&d.outputs[b].channelBuffers64[c])d.outputs[b].channelBuffers64[c][f]=sample;
   if(sample!=0)d.outputs[b].silenceFlags&=~(1ULL<<c);
  }
 }
 // Preserve each bounded automation point and its exact host sample offset.
 // Only XML-backed ordinary CCs are transmitted. Other parameters stay local.
 int32 parameterBudget=4096;
 if(d.inputParameterChanges) {
  const auto n=std::min(d.inputParameterChanges->getParameterCount(),static_cast<int32>(kCatalog.size()));
  for(int32 i=0;i<n;++i) {
   auto* q=d.inputParameterChanges->getParameterData(i);if(!q)continue;
   auto index=indexOf(q->getParameterId());if(index==kCatalog.size()||!kCatalog[index].resolved)continue;
   const auto count=q->getPointCount();
   for(int32 point=0;point<count&&parameterBudget>0;++point,--parameterBudget) {
    int32 offset=0;ParamValue value=0;
    if(q->getPoint(point,offset,value)!=kResultTrue||!std::isfinite(value)||value<0||value>1||offset<0||(d.numSamples==0?offset!=0:offset>=d.numSamples))continue;
    values_[index].store(value,std::memory_order_relaxed);
    const auto& spec=kCatalog[index];
    if(spec.midiCC>=0&&active_&&processing_&&d.numSamples>0&&d.processMode!=kOffline) {
     std::array<uint8_t,3> bytes{};
     auto discrete=static_cast<unsigned>(std::lround(spec.minimum+value*(spec.maximum-spec.minimum)));
     if(virus::midi::build_control_change(spec.part-1,spec.midiCC,discrete,bytes))bridge_->send(hostFrame_,offset,bytes);
    }
   }
  }
 }
 // Bounded copied MIDI messages; borrowed SysEx is not retained.
 if(d.inputEvents) {
  const auto count=d.inputEvents->getEventCount();const auto n=std::min(count,4096);
  if(count>n)rejected_+=static_cast<uint64_t>(count-n);
  for(int32 i=0;i<n;++i) {
   Event event{};bool valid=d.inputEvents->getEvent(i,event)==kResultTrue&&event.busIndex==0&&event.sampleOffset>=0&&event.sampleOffset<d.numSamples;
   if(valid) switch(event.type) {
    case Event::kNoteOnEvent: valid=event.noteOn.channel>=0&&event.noteOn.channel<16&&event.noteOn.pitch>=0&&event.noteOn.pitch<128&&std::isfinite(event.noteOn.velocity)&&event.noteOn.velocity>=0&&event.noteOn.velocity<=1; break;
    case Event::kNoteOffEvent: valid=event.noteOff.channel>=0&&event.noteOff.channel<16&&event.noteOff.pitch>=0&&event.noteOff.pitch<128&&std::isfinite(event.noteOff.velocity)&&event.noteOff.velocity>=0&&event.noteOff.velocity<=1; break;
    case Event::kPolyPressureEvent: valid=event.polyPressure.channel>=0&&event.polyPressure.channel<16&&event.polyPressure.pitch>=0&&event.polyPressure.pitch<128&&std::isfinite(event.polyPressure.pressure)&&event.polyPressure.pressure>=0&&event.polyPressure.pressure<=1; break;
    default: valid=false;break; // SysEx and other borrowed data are not retained.
   }
   if(valid) {
    ++events_;
    std::array<uint8_t,3> bytes{};
    if(event.type==Event::kNoteOnEvent)bytes={static_cast<uint8_t>(0x90|event.noteOn.channel),static_cast<uint8_t>(event.noteOn.pitch),static_cast<uint8_t>(std::lround(event.noteOn.velocity*127))};
    else if(event.type==Event::kNoteOffEvent)bytes={static_cast<uint8_t>(0x80|event.noteOff.channel),static_cast<uint8_t>(event.noteOff.pitch),static_cast<uint8_t>(std::lround(event.noteOff.velocity*127))};
    else virus::midi::build_poly_pressure(event.polyPressure.channel,event.polyPressure.pitch,static_cast<unsigned>(std::lround(event.polyPressure.pressure*127)),bytes);
    if(active_&&processing_&&d.processMode!=kOffline)bridge_->send(hostFrame_,event.sampleOffset,bytes);
   } else ++rejected_;
  }
 }
 hostFrame_+=static_cast<uint64_t>(d.numSamples);
 bridge_->completeBlock(hostFrame_);
 return kResultOk;
}
tresult PLUGIN_API Processor::getState(IBStream* stream) {
 if(!stream)return kInvalidArgument;
 ProjectPatches project;bool reconnect=false;
 if(!hardware_->captureProject(project,reconnect))return kResultFalse;
 Snapshot snapshot{};for(size_t i=0;i<snapshot.size();++i)snapshot[i]=value(i);for(unsigned p=0;p<16;++p)patchControls(snapshot,project.parts[p],p);
 IBStreamer s(stream,kLittleEndian);
 if(!s.writeInt32u(kMagic)||!s.writeInt32u(kVersion)||!s.writeInt32u(kCatalog.size()))return kResultFalse;
 for(size_t i=0;i<kCatalog.size();++i)if(!s.writeInt32u(kCatalog[i].id)||!s.writeDouble(snapshot[i]))return kResultFalse;
 if(!s.writeInt32u(reconnect&&!project.empty()?1:0)||!s.writeInt32u(16))return kResultFalse;
 auto writePatch=[&](const PatchBytes& patch){if(!s.writeInt32u(patch.size()))return false;for(auto byte:patch)if(!s.writeInt8u(byte))return false;return s.writeInt32u(patchCRC(patch));};
 for(auto& patch:project.parts)if(!writePatch(patch))return kResultFalse;
 return writePatch(project.multi)?kResultOk:kResultFalse;
}
tresult PLUGIN_API Processor::setState(IBStream* stream) {
 Snapshot snapshot{};ProjectPatches project;bool reconnect=false;if(!readState(stream,snapshot,project,reconnect))return kResultFalse;
 std::array<uint8_t,16> volumes{};volumes.fill(100);for(size_t i=0;i<kCatalog.size();++i)if(kCatalog[i].midiCC==7)volumes[kCatalog[i].part-1]=uint8_t(std::lround(snapshot[i]*127));
 if(!hardware_->restoreProject(project,reconnect,&volumes))return kResultFalse;
 for(size_t i=0;i<snapshot.size();++i)values_[i].store(snapshot[i],std::memory_order_relaxed);
 return kResultOk;
}
IPlugView* PLUGIN_API Controller::createView(FIDString type) {return type&&std::strcmp(type,ViewType::kEditor)==0?createNativeEditor(this):nullptr;}
tresult PLUGIN_API Controller::initialize(FUnknown* context) {
 auto r=EditControllerEx1::initialize(context);if(r!=kResultOk)return r;
 for(int part=1;part<=16;++part){char name[32];std::snprintf(name,sizeof name,"Part %d",part);String128 text;UString(text,128).fromAscii(name);addUnit(new Unit(text,part,kRootUnitId));}
 for(const auto& p:kCatalog) {
  String128 name;UString(name,128).fromAscii(p.name);
  parameters.addParameter(new RangeParameter(name,p.id,nullptr,p.minimum,p.maximum,p.initial,p.resolved?static_cast<int32>(p.maximum-p.minimum):0,p.resolved?ParameterInfo::kCanAutomate:ParameterInfo::kIsReadOnly,p.part));
 }
 auto* status=new StringListParameter(STR16("Hardware connection"),kConnectionStatus,nullptr,ParameterInfo::kIsReadOnly);
 status->appendString(STR16("Disconnected - offline development"));parameters.addParameter(status);
 // Poll on the controller's UI run loop, even with the editor closed. API
 // changes occur outside performEdit(), so hosts need an explicit dirty
 // notification before deciding whether to reuse cached component state.
 stateTimer=owned(Timer::create(this,500));
 return kResultOk;
}
Controller::~Controller(){if(stateTimer)stateTimer->stop();stateTimer=nullptr;}
tresult PLUGIN_API Controller::terminate(){if(stateTimer)stateTimer->stop();stateTimer=nullptr;return EditControllerEx1::terminate();}
void Controller::onTimer(Timer*){auto message=owned(allocateMessage());if(message){message->setMessageID("VTRStateRevision");sendMessage(message);}}
tresult PLUGIN_API Controller::setComponentState(IBStream* stream) {
 Snapshot snapshot{};ProjectPatches project;bool reconnect=false;if(!readState(stream,snapshot,project,reconnect))return kResultFalse;
 hardwarePatches=std::move(project);hardwarePatch=hardwarePatches.parts[0];++patchRevision;
 for(size_t i=0;i<snapshot.size();++i)setParamNormalized(kCatalog[i].id,snapshot[i]);
 return kResultOk;
}
tresult PLUGIN_API Controller::getMidiControllerAssignment(int32 bus,int16 channel,CtrlNumber cc,ParamID& id) {
 if(bus!=0||channel<0||channel>=16||cc<0||cc>127)return kResultFalse;
 for(const auto& p:kCatalog)if(p.part==channel+1&&p.midiCC==cc){id=p.id;return kResultTrue;}
 return kResultFalse;
}
}
#ifndef VIRUS_REWORK_TEST
using namespace Steinberg;
using namespace Steinberg::Vst;
BEGIN_FACTORY_DEF("Virus TI Rework Project","","")
DEF_CLASS2(INLINE_UID_FROM_FUID(VirusRework::kProcessorID),PClassInfo::kManyInstances,kVstAudioEffectClass,"Virus TI Rework",0,"Instrument|External","0.2.0-beta.1",kVstVersionString,VirusRework::Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(VirusRework::kControllerID),PClassInfo::kManyInstances,kVstComponentControllerClass,"Virus TI Rework Controller",0,"","0.2.0-beta.1",kVstVersionString,VirusRework::Controller::create)
END_FACTORY
#endif
