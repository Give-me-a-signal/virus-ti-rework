#include "plugin.h"
#include "midi_safety.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
#include <new>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "pluginterfaces/vst/ivstprocesscontext.h"
thread_local bool inProcess = false;
static size_t processAllocations = 0;
void* operator new(std::size_t size) {if(inProcess) ++processAllocations; if(auto* p=std::malloc(size ? size : 1))return p; throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete[](void* p) noexcept {std::free(p);}
using namespace VirusRework;
static tresult process(Processor& processor, ProcessData& data) {inProcess=true;const auto result=processor.process(data);inProcess=false;return result;}
#define CHECK(x) do {if(!(x)){std::cerr<<"Failed line "<<__LINE__<<": "#x<<"\n";std::abort();}}while(0)
static void connectedBridgeTests() {
 virus::bridge::DeviceBridge bridge;
 Processor p,other;
 CHECK(p.initialize(nullptr)==kResultOk);CHECK(other.initialize(nullptr)==kResultOk);
 // Exercise only the FUnknown interface a dynamically loaded host can query.
 virus::bridge::IVirusDeviceBridge* bridgeInterface=nullptr;
 IComponent* component=&p;
 CHECK(component->queryInterface(virus::bridge::IVirusDeviceBridge::iid,reinterpret_cast<void**>(&bridgeInterface))==kResultOk);
 CHECK(bridgeInterface);CHECK(bridgeInterface->attachDeviceBridge(&bridge)==kResultOk);
 CHECK(!other.attachBridge(bridge));CHECK(!bridge.ready());
 CHECK(bridgeInterface->attachDeviceBridge(nullptr)==kResultOk);
 CHECK(other.attachBridge(bridge));CHECK(bridgeInterface->attachDeviceBridge(&bridge)==kResultFalse);
 CHECK(other.attachDeviceBridge(nullptr)==kResultOk);
 CHECK(bridgeInterface->attachDeviceBridge(&bridge)==kResultOk);
 ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;setup.sampleRate=48000;setup.maxSamplesPerBlock=128;
 CHECK(p.setupProcessing(setup)==kResultOk);CHECK(p.setActive(true)==kResultOk);CHECK(p.setProcessing(true)==kResultOk);
 CHECK(!p.attachBridge(bridge));CHECK(bridgeInterface->attachDeviceBridge(nullptr)==kResultFalse);
 auto generation=bridge.generation();CHECK(bridge.serviceReady(generation));
 // Actual Engine queue -> bridge -> real VST processor, with a synthetic,
 // explicit one-to-one host frame assignment supplied only by this test.
 virus::AudioFrameQueue capture(16,6);
 std::array<int32_t,48> pcm{};
 for(int f=0;f<8;++f)for(int c=0;c<6;++c)pcm[f*6+c]=(c%2?-1:1)*(c+1)*0x10000000;
 CHECK(capture.push(pcm.data(),8,48)==virus::QueueStatus::ok);
 for(unsigned f=0;f<8;++f)CHECK(bridge.serviceTransferCapture(capture,generation,f));
 float samples[6][128]{};float* channels[3][2];AudioBusBuffers buses[3]{};
 for(int b=0;b<3;++b){channels[b][0]=samples[b*2];channels[b][1]=samples[b*2+1];buses[b].numChannels=2;buses[b].channelBuffers32=channels[b];}
 EventList events;Event e{};e.type=Event::kNoteOnEvent;e.noteOn.channel=15;e.noteOn.pitch=64;e.noteOn.velocity=1;e.sampleOffset=7;events.addEvent(e);
 e.type=Event::kNoteOffEvent;e.noteOff.channel=1;e.noteOff.pitch=65;e.noteOff.velocity=.5f;e.sampleOffset=3;events.addEvent(e);
 e.type=Event::kPolyPressureEvent;e.polyPressure.channel=2;e.polyPressure.pitch=66;e.polyPressure.pressure=1;e.sampleOffset=5;events.addEvent(e);
 ParameterChanges changes;int32 qi=0,pi=0;ParamID cutoff=0;
 for(const auto& spec:kCatalog)if(spec.part==1&&spec.midiCC==40){cutoff=spec.id;break;}
 CHECK(cutoff);auto* queue=changes.addParameterData(cutoff,qi);queue->addPoint(2,.5,pi);queue->addPoint(6,1,pi);
 ProcessContext context{};context.state=ProcessContext::kPlaying;context.projectTimeSamples=1000;
 ProcessData data{};data.numSamples=8;data.symbolicSampleSize=kSample32;data.numOutputs=3;data.outputs=buses;data.inputEvents=&events;data.inputParameterChanges=&changes;data.processContext=&context;
 CHECK(process(p,data)==kResultOk);
 for(int b=0;b<3;++b)CHECK(buses[b].silenceFlags==0);
 for(int f=0;f<8;++f)for(int c=0;c<6;++c)CHECK(samples[c][f]==pcm[f*6+c]/2147483648.0f);
 virus::bridge::MidiMessage m;
 CHECK(bridge.serviceTakeMidi(m));CHECK(m.bytes==(std::array<uint8_t,3>{0xb0,40,64}));CHECK(m.sample_offset==2&&m.block_start==0&&m.generation==generation);CHECK(m.domain==virus::bridge::ClockDomain::plugin_sample_timeline);
 CHECK(bridge.serviceTakeMidi(m));CHECK(m.bytes==(std::array<uint8_t,3>{0xb0,40,127}));CHECK(m.sample_offset==6);
 CHECK(bridge.serviceTakeMidi(m));CHECK(m.bytes==(std::array<uint8_t,3>{0x9f,64,127}));CHECK(m.sample_offset==7);
 CHECK(bridge.serviceTakeMidi(m));CHECK(m.bytes==(std::array<uint8_t,3>{0x81,65,64}));CHECK(m.sample_offset==3);
 CHECK(bridge.serviceTakeMidi(m));CHECK(m.bytes==(std::array<uint8_t,3>{0xa2,66,127}));CHECK(m.sample_offset==5);CHECK(!bridge.serviceTakeMidi(m));
 // Partial underrun: one future frame waits while missing leading frame is zero.
 virus::bridge::AudioFrame frame;frame.generation=generation;frame.host_frame=9;frame.pcm.fill(0x40000000);CHECK(bridge.servicePublish(frame));
 double doubles[6][128]{};double* dchannels[3][2];
 for(int b=0;b<3;++b){dchannels[b][0]=doubles[b*2];dchannels[b][1]=doubles[b*2+1];buses[b].channelBuffers64=dchannels[b];}
 data.symbolicSampleSize=kSample64;data.inputEvents=nullptr;data.inputParameterChanges=nullptr;context.projectTimeSamples=1008;
 CHECK(process(p,data)==kResultOk);for(int c=0;c<6;++c){CHECK(doubles[c][0]==0);CHECK(doubles[c][1]==.5);for(int f=2;f<8;++f)CHECK(doubles[c][f]==0);}
 CHECK(p.bridgeStats().underrun==7);
 // A project seek invalidates both audio and pending MIDI from the old epoch.
 frame.host_frame=16;CHECK(bridge.servicePublish(frame));
 CHECK(bridge.send(8,7,{0x90,60,127}));context.projectTimeSamples=-500;
 CHECK(process(p,data)==kResultOk);CHECK(!bridge.ready());CHECK(!bridge.serviceTakeMidi(m));CHECK(!bridge.servicePublish(frame));
 generation=bridge.generation();CHECK(bridge.serviceReady(generation));
 frame.generation=generation;frame.host_frame=24;CHECK(bridge.servicePublish(frame));context.projectTimeSamples=-492;
 CHECK(process(p,data)==kResultOk);for(int c=0;c<6;++c)CHECK(doubles[c][0]==.5);CHECK(p.bridgeStats().discarded_audio==1);
 // Disconnect cannot re-ready the same epoch or replay buffered audio/notes.
 CHECK(bridge.send(24,1,{0x90,60,127}));frame.host_frame=32;CHECK(bridge.servicePublish(frame));bridge.serviceDisconnect();CHECK(!bridge.serviceReady(generation));CHECK(!bridge.serviceTakeMidi(m));context.projectTimeSamples=-484;
 CHECK(process(p,data)==kResultOk);for(int c=0;c<6;++c)for(int f=0;f<8;++f)CHECK(doubles[c][f]==0);
 CHECK(bridge.generation()!=generation);CHECK(!bridge.servicePublish(frame));
 generation=bridge.generation();CHECK(bridge.serviceReady(generation));
 // Offline export must never send notes or CC automation to real hardware.
 data.processMode=kOffline;data.inputEvents=&events;data.inputParameterChanges=&changes;data.processContext=nullptr;
 CHECK(process(p,data)==kResultOk);CHECK(!bridge.serviceTakeMidi(m));
 data.processMode=kRealtime;data.inputEvents=nullptr;data.inputParameterChanges=nullptr;
 // A full queue drops explicitly and never allocates. Stopping blocks publication.
 for(unsigned i=0;i<2048;++i)CHECK(bridge.send(40,i,{0x90,60,127}));CHECK(!bridge.send(40,2048,{0x90,60,127}));CHECK(p.bridgeStats().midi_dropped==1);
 CHECK(p.setProcessing(false)==kResultOk);CHECK(!bridge.serviceReady(generation));CHECK(!bridge.serviceTakeMidi(m));CHECK(p.setActive(false)==kResultOk);
 CHECK(bridgeInterface->attachDeviceBridge(nullptr)==kResultOk);CHECK(other.attachBridge(bridge));
 bridgeInterface->release();
 p.terminate();other.terminate();
}
static void localApiTests(){
 virus::bridge::DeviceBridge bridge;
 HardwareService service(bridge);
 CHECK(service.stateRevision()==0);
 auto endpoint=service.controlEndpoint();CHECK(!endpoint.empty());
 auto request=[&](const char* command){
  int fd=socket(AF_UNIX,SOCK_STREAM,0);CHECK(fd>=0);sockaddr_un address{};address.sun_family=AF_UNIX;std::strcpy(address.sun_path,endpoint.c_str());CHECK(connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);
  timeval timeout{2,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));CHECK(send(fd,command,std::strlen(command),0)==std::strlen(command));
  std::string response;char buf[4096];for(;;){auto n=recv(fd,buf,sizeof(buf),0);CHECK(n>0);response.append(buf,n);if(response.back()=='\n')break;}close(fd);return response;
 };
 auto status=request("status\n");CHECK(status.find("\"connected\":false")!=std::string::npos);CHECK(status.find("\"capture_callbacks\":0")!=std::string::npos);
 CHECK(status.find("\"firmware_operations_supported\":false")!=std::string::npos);
 CHECK(request("flash\n").find("Unknown command")!=std::string::npos);
 CHECK(request("connect\n").find("inactive")!=std::string::npos); // Scanner cannot start hardware.
 CHECK(request("test-tone\n").find("inactive")!=std::string::npos);
 CHECK(!service.loadPreset({0xf0,0,0x20,0x33,1,16,0x7f,0xf7}));
 CHECK(service.stateRevision()==0);
 std::vector<uint8_t> preset(524,0);preset[0]=0xf0;preset[2]=0x20;preset[3]=0x33;preset[4]=1;preset[5]=16;preset[6]=16;preset[9]=12;preset.back()=0xf7;
 unsigned sum=0;for(size_t i=5;i<265;i++)sum+=preset[i];preset[265]=sum&127;sum=0;for(size_t i=5;i<522;i++)sum+=preset[i];preset[522]=sum&127;
 CHECK(service.loadPreset(preset));CHECK(service.status().find("staged")!=std::string::npos);
 CHECK(service.stateRevision()==1);
 CHECK(request("status\n").find("\"capture_callbacks\":0")!=std::string::npos);
 CHECK(!service.connected());
 ProjectPatches captured;bool reconnect=true;
 CHECK(service.captureProject(captured,reconnect));CHECK(captured.parts[0]==preset);CHECK(!reconnect);
 CHECK(service.stateRevision()==1); // Reading/saving must not dirty the host again.
 CHECK(service.restoreProject(captured,true));CHECK(!service.connected());
 CHECK(service.captureProject(captured,reconnect));CHECK(captured.parts[0]==preset);CHECK(reconnect);
 CHECK(request("status\n").find("\"state_recall_pending\":true")!=std::string::npos);
}
static void dirtyNotificationTests(){
 struct Handler final : FObject,IComponentHandler,IComponentHandler2 {
  int dirtyCalls=0;
  tresult PLUGIN_API queryInterface(const TUID iid,void** obj)override{
   QUERY_INTERFACE(iid,obj,IComponentHandler::iid,IComponentHandler)
   QUERY_INTERFACE(iid,obj,IComponentHandler2::iid,IComponentHandler2)
   return FObject::queryInterface(iid,obj);
  }
  DELEGATE_REFCOUNT(FObject)
  tresult PLUGIN_API beginEdit(ParamID)override{return kResultOk;}
  tresult PLUGIN_API performEdit(ParamID,ParamValue)override{return kResultOk;}
  tresult PLUGIN_API endEdit(ParamID)override{return kResultOk;}
  tresult PLUGIN_API restartComponent(int32)override{return kResultOk;}
  tresult PLUGIN_API setDirty(TBool state)override{if(state)++dirtyCalls;return kResultOk;}
  tresult PLUGIN_API requestOpenEditor(FIDString)override{return kResultOk;}
  tresult PLUGIN_API startGroupEdit()override{return kResultOk;}
  tresult PLUGIN_API finishGroupEdit()override{return kResultOk;}
 } handler;
 Controller controller;CHECK(controller.initialize(nullptr)==kResultOk);
 CHECK(controller.setComponentHandler(&handler)==kResultOk);
 HostMessage message;message.setMessageID("VTRStateRevision");
 // No editor is created. An API change must still invalidate a host's cache,
 // while idle polls and repeated saves must not repeatedly dirty the project.
 for(int64 revision:{0,1,1,2,2}){message.getAttributes()->setInt("revision",revision);CHECK(controller.notify(&message)==kResultOk);}
 CHECK(handler.dirtyCalls==2);
 CHECK(controller.terminate()==kResultOk);
}
int main() {
 dirtyNotificationTests();
 // Expanding USB routing must not allow arbitrary global or firmware writes.
 for(unsigned value=0;value<128;++value){
  bool accepted=true;try{validateMidi({0xf0,0,0x20,0x33,1,16,0x73,0,9,uint8_t(value),0xf7});}catch(...){accepted=false;}
  CHECK(accepted==(value<=1));
 }
 for(const auto& message:std::vector<std::vector<uint8_t>>{
     {0xf0,0,0x20,0x33,1,16,0x73,1,9,1,0xf7},
     {0xf0,0,0x20,0x33,1,16,0x73,0,10,1,0xf7},
     {0xf0,0,0x20,0x33,1,16,0x50,0,0,0xf7}}){
  bool rejected=false;try{validateMidi(message);}catch(...){rejected=true;}CHECK(rejected);
 }
 localApiTests();
 Processor p;Controller c;CHECK(p.initialize(nullptr)==kResultOk);CHECK(c.initialize(nullptr)==kResultOk);
 CHECK(c.getParameterCount()==2673);CHECK(p.getBusCount(kAudio,kOutput)==3);CHECK(p.getBusCount(kEvent,kInput)==1);
 CHECK(p.getBusCount(kAudio,kInput)==0);
 SpeakerArrangement arrangements[3]={SpeakerArr::kStereo,SpeakerArr::kStereo,SpeakerArr::kStereo};
 CHECK(p.setBusArrangements(nullptr,0,arrangements,3)==kResultOk);arrangements[1]=SpeakerArr::kMono;
 CHECK(p.setBusArrangements(nullptr,0,arrangements,3)==kResultFalse);
 ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;setup.sampleRate=48000;setup.maxSamplesPerBlock=128;CHECK(p.setupProcessing(setup)==kResultOk);
 float left[128],right[128];std::fill_n(left,128,1);std::fill_n(right,128,-1);float* channels[2]={left,right};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;
 ParameterChanges changes;int32 queueIndex=0,pointIndex=0;auto* queue=changes.addParameterData(kCatalog[0].id,queueIndex);queue->addPoint(0,.25,pointIndex);queue->addPoint(127,.75,pointIndex);
 EventList events;Event e{};e.type=Event::kNoteOnEvent;e.noteOn.channel=15;e.noteOn.pitch=64;e.noteOn.velocity=.8f;e.sampleOffset=127;CHECK(events.addEvent(e)==kResultOk);
 e.noteOn.channel=16;events.addEvent(e);e.noteOn.channel=0;e.sampleOffset=128;events.addEvent(e);e.sampleOffset=0;e.noteOn.velocity=std::numeric_limits<float>::quiet_NaN();events.addEvent(e);
 ProcessData data{};data.numSamples=128;data.symbolicSampleSize=kSample32;data.numOutputs=1;data.outputs=&bus;data.inputParameterChanges=&changes;data.inputEvents=&events;
 CHECK(process(p,data)==kResultOk);CHECK(p.value(0)==.75);CHECK(p.acceptedEvents()==1);CHECK(p.rejectedEvents()==3);CHECK(bus.silenceFlags==3);
 for(int i=0;i<128;++i){CHECK(left[i]==0);CHECK(right[i]==0);}
 queue->addPoint(10,std::numeric_limits<double>::quiet_NaN(),pointIndex);CHECK(process(p,data)==kResultOk);CHECK(p.value(0)==.75);
 data.inputEvents=nullptr;data.inputParameterChanges=nullptr;data.numSamples=0;data.numOutputs=0;data.outputs=nullptr;CHECK(process(p,data)==kResultOk);
 double dl[128],dr[128];std::fill_n(dl,128,3);std::fill_n(dr,128,4);double* dc[2]={dl,dr};bus.channelBuffers64=dc;data.outputs=&bus;data.numOutputs=1;data.numSamples=128;data.symbolicSampleSize=kSample64;CHECK(process(p,data)==kResultOk);for(int i=0;i<128;++i){CHECK(dl[i]==0);CHECK(dr[i]==0);}
 data.numSamples=-1;CHECK(process(p,data)==kInvalidArgument);data.numSamples=129;CHECK(process(p,data)==kInvalidArgument);
 MemoryStream state;CHECK(p.getState(&state)==kResultOk);CHECK(state.getSize()==156+2672*12);
 Processor restored;state.seek(0,IBStream::kIBSeekSet,nullptr);CHECK(restored.setState(&state)==kResultOk);CHECK(restored.value(0)==.75);
 state.seek(0,IBStream::kIBSeekSet,nullptr);CHECK(c.setComponentState(&state)==kResultOk);CHECK(c.getParamNormalized(kCatalog[0].id)==.75);
 std::vector<char> raw(state.getData(),state.getData()+state.getSize());
 // Every possible truncation of the binary state must fail without partial mutation.
 for(size_t size=0;size<raw.size();++size){MemoryStream truncated(raw.data(),size);CHECK(restored.setState(&truncated)==kResultFalse);CHECK(restored.value(0)==.75);}
 auto damaged=raw;damaged[4]=99;MemoryStream version(damaged.data(),damaged.size());CHECK(restored.setState(&version)==kResultFalse);
 damaged=raw;damaged[12]^=1;MemoryStream unknownID(damaged.data(),damaged.size());CHECK(restored.setState(&unknownID)==kResultFalse);
 damaged=raw;double nan=std::numeric_limits<double>::quiet_NaN();std::memcpy(damaged.data()+16,&nan,8);MemoryStream invalid(damaged.data(),damaged.size());CHECK(restored.setState(&invalid)==kResultFalse);
 // Version 1 states remain readable with their original mapped values.
 auto legacy=raw;legacy.resize(12+2672*12);legacy[4]=1;
 MemoryStream oldState(legacy.data(),legacy.size());CHECK(restored.setState(&oldState)==kResultOk);CHECK(restored.value(0)==.75);
 // A complete TI patch survives the actual processor + controller state paths.
 std::vector<uint8_t> full(524,0);full[0]=0xf0;full[2]=0x20;full[3]=0x33;full[4]=1;full[5]=16;full[6]=16;full[9]=12;full.back()=0xf7;
 for(size_t i=13;i<522;++i)full[i]=uint8_t((i*17)%128);
 unsigned sum=0;for(size_t i=5;i<265;++i)sum+=full[i];full[265]=sum&127;sum=0;for(size_t i=5;i<522;++i)sum+=full[i];full[522]=sum&127;
 auto fullState=raw;fullState.resize(12+2672*12);fullState[4]=2;
 auto append32=[&](uint32_t v){for(int i=0;i<4;++i)fullState.push_back(char(v>>(i*8)));};
 append32(1);append32(full.size());fullState.insert(fullState.end(),full.begin(),full.end());
 uint32_t crc=~0u;for(auto byte:full){crc^=byte;for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int(crc&1)));}append32(~crc);
 MemoryStream fullInput(fullState.data(),fullState.size());CHECK(restored.setState(&fullInput)==kResultOk);
 fullInput.seek(0,IBStream::kIBSeekSet,nullptr);CHECK(c.setComponentState(&fullInput)==kResultOk);CHECK(c.hardwarePatch==full);
 MemoryStream fullSaved;CHECK(restored.getState(&fullSaved)==kResultOk);CHECK(fullSaved.getSize()==156+2672*12+524);
 CHECK(std::memcmp(fullSaved.getData()+12+2672*12+12,full.data(),full.size())==0);
 // Every payload truncation and a CRC bit flip fail atomically.
 for(size_t n=12+2672*12;n<fullState.size();++n){MemoryStream truncated(fullState.data(),n);CHECK(restored.setState(&truncated)==kResultFalse);}
 auto corrupt=fullState;corrupt.back()^=1;MemoryStream corruptStream(corrupt.data(),corrupt.size());CHECK(restored.setState(&corruptStream)==kResultFalse);
 MemoryStream unchanged;CHECK(restored.getState(&unchanged)==kResultOk);CHECK(unchanged.getSize()==fullSaved.getSize());CHECK(std::memcmp(unchanged.getData(),fullSaved.getData(),fullSaved.getSize())==0);
 // Version 3 holds independent full patches for every part and a checksum-checked multi.
 auto sixteen=raw;sixteen.resize(12+2672*12);
 auto u32=[&](uint32_t value){for(int i=0;i<4;++i)sixteen.push_back(char(value>>(8*i)));};
 auto blob=[&](const std::vector<uint8_t>& bytes){u32(bytes.size());sixteen.insert(sixteen.end(),bytes.begin(),bytes.end());uint32_t crc=~0u;for(auto b:bytes){crc^=b;for(int n=0;n<8;++n)crc=(crc>>1)^(0xedb88320u&uint32_t(-int(crc&1)));}u32(~crc);};
 u32(1);u32(16);ProjectPatches expected;
 for(unsigned part=0;part<16;++part){auto patch=full;patch[49]=part+20;patch[249]='A'+part;sum=0;for(size_t i=5;i<265;++i)sum+=patch[i];patch[265]=sum&127;sum=0;for(size_t i=5;i<522;++i)sum+=patch[i];patch[522]=sum&127;patch=virus::patches::ram_single(virus::patches::parse_single(patch),part);expected.parts[part]=patch;blob(patch);}
 auto multi=std::vector<uint8_t>(267,0);multi[0]=0xf0;multi[2]=0x20;multi[3]=0x33;multi[4]=1;multi[5]=16;multi[6]=17;multi[8]=127;multi.back()=0xf7;multiChecksum(multi);blob(multi);
 MemoryStream sixteenInput(sixteen.data(),sixteen.size());CHECK(restored.setState(&sixteenInput)==kResultOk);
 MemoryStream sixteenSaved;CHECK(restored.getState(&sixteenSaved)==kResultOk);CHECK(sixteenSaved.getSize()==40871);
 sixteenSaved.seek(0,IBStream::kIBSeekSet,nullptr);CHECK(c.setComponentState(&sixteenSaved)==kResultOk);
 for(unsigned part=0;part<16;++part)CHECK(c.hardwarePatches.parts[part]==expected.parts[part]);
 for(size_t size=12+2672*12;size<sixteen.size();++size){MemoryStream truncated(sixteen.data(),size);CHECK(restored.setState(&truncated)==kResultFalse);}
 // A four-part project's inactive slots remain empty in the existing v3
 // format. Ten real processor/controller round trips must not append records,
 // populate parts 5–16, change routing or connect to hardware while dormant.
 sixteen=raw;sixteen.resize(12+2672*12);u32(0);u32(16);
 for(unsigned part=0;part<16;++part)blob(part<4?expected.parts[part]:PatchBytes{});
 blob(multi);
 for(unsigned repeat=0;repeat<10;++repeat){
  MemoryStream input(sixteen.data(),sixteen.size());CHECK(restored.setState(&input)==kResultOk);
  MemoryStream saved;CHECK(restored.getState(&saved)==kResultOk);
  CHECK(saved.getSize()==156+2672*12+4*524+267);
  saved.seek(0,IBStream::kIBSeekSet,nullptr);CHECK(c.setComponentState(&saved)==kResultOk);
  for(unsigned part=0;part<16;++part)CHECK(c.hardwarePatches.parts[part]==(part<4?expected.parts[part]:PatchBytes{}));
  CHECK(c.hardwarePatches.multi==multi);
  if(repeat)CHECK(sixteen.size()==size_t(saved.getSize())&&std::memcmp(sixteen.data(),saved.getData(),sixteen.size())==0);
  sixteen.assign(saved.getData(),saved.getData()+saved.getSize());
 }
 // A separate processor does not inherit this instance's saved patch.
 MemoryStream isolated;CHECK(p.getState(&isolated)==kResultOk);CHECK(isolated.getSize()==156+2672*12);
 ParamID id=0;CHECK(c.getMidiControllerAssignment(0,15,1,id)==kResultTrue);CHECK(c.getMidiControllerAssignment(0,16,1,id)==kResultFalse);CHECK(c.getMidiControllerAssignment(1,0,1,id)==kResultFalse);CHECK(c.getMidiControllerAssignment(0,0,kPitchBend,id)==kResultFalse);

 // Flood handling examines at most 4096 events and accounts for the remainder.
 EventList flood(5000);e={};e.type=Event::kNoteOffEvent;e.noteOff.channel=0;e.noteOff.pitch=60;e.noteOff.velocity=0;
 for(int i=0;i<5000;++i)CHECK(flood.addEvent(e)==kResultOk);
 const auto accepted=p.acceptedEvents(), rejected=p.rejectedEvents();
 data.numSamples=128;data.numOutputs=0;data.outputs=nullptr;data.inputEvents=&flood;
 CHECK(process(p,data)==kResultOk);CHECK(p.acceptedEvents()==accepted+4096);CHECK(p.rejectedEvents()==rejected+904);
 connectedBridgeTests();
 CHECK(processAllocations==0);
 p.terminate();c.terminate();
 std::cout<<"PASS: native processor/controller, nonzero six-channel PCM, exact MIDI offsets, bridge lifecycle, underrun, discontinuity, zero process allocations, catalog, state and "<<raw.size()<<" state truncations\n";
}
