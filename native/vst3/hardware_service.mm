#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#include "hardware_service.h"
#include "capture_fifo.h"
#include "../device-identity/include/virus/device_identity.hpp"
#include "local_control.h"
#include "browser_bank.hpp"
#include "device_session.h"
#include "restoration_guard.h"
#include "device_lease.h"
#include "recall_policy.h"
#include "restoration.h"
#include "performance_queue.h"
#include "../clock-sync/include/virus/marker_clock.hpp"
#include "project_recall.h"
#include <dlfcn.h>
#include "../private-midi/hardware_midi.hpp"
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

@interface VTRHardwareBundleMarker : NSObject @end
@implementation VTRHardwareBundleMarker @end

namespace VirusRework {
using virus::hardware::Midi;
using virus::hardware::check;
namespace {
#ifdef VTR_TIMING_DAW_TEST_BUILD
constexpr bool timingDawTestBuild=true;
#else
constexpr bool timingDawTestBuild=false;
#endif
NSString* resources(){auto root=[[NSBundle bundleForClass:VTRHardwareBundleMarker.class]resourcePath];
#ifdef VTR_BUNDLE_RESOURCES
 if(![[NSFileManager defaultManager]fileExistsAtPath:[root stringByAppendingPathComponent:@"PatchBanks"]])root=@VTR_BUNDLE_RESOURCES;
#endif
 return root;
}
class HardwareMidi final:public DeviceSession{
 Midi midi;
public:
 explicit HardwareMidi(bool snow=false):midi(snow){midi.usePrivate(snow);}
 void send(const PatchBytes& p)override{midi.send(p);}
 void sendTimed(const PatchBytes& p,uint16_t stamp,bool clock)override{midi.sendTimed(p,stamp,clock);}
 void sendTimedBatch(const std::vector<TimedMessage>& batch)override{midi.sendTimedBatch(batch);}
 PatchBytes single(uint8_t part,const std::function<void()>& idle)override{return midi.single(part,idle);}
 PatchBytes multi()override{return midi.multi();}
 void saveTraffic(const std::string& path)override{midi.savePrivate(@(path.c_str()));}
 uint64_t registryID()const{return midi.registryID();}
};
AudioDeviceID findDevice(DeviceModel& model,bool snowOnly){
 AudioObjectPropertyAddress a{kAudioHardwarePropertyDevices,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};UInt32 size=0;
 check(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,&a,0,nullptr,&size),"Enumerate audio");
 std::vector<AudioDeviceID> devices(size/sizeof(AudioDeviceID));check(AudioObjectGetPropertyData(kAudioObjectSystemObject,&a,0,nullptr,&size,devices.data()),"Read audio devices");
 std::vector<virus::identity::AudioCandidate> candidates;
 for(auto d:devices){CFStringRef name=nullptr;size=sizeof(name);a.mSelector=kAudioObjectPropertyName;
  check(AudioObjectGetPropertyData(d,&a,0,nullptr,&size,&name),"Audio name");
  const char* label=name?[(__bridge NSString*)name UTF8String]:nullptr;
  auto model=virus::identity::family(label?label:"");if(name)CFRelease(name);
  candidates.push_back({d,model});}
 // Select one recognized model before opening MIDI/capture. Snow uses its own
 // four-part contract in every build; multiple-device ambiguity is still fatal.
 const AudioDeviceID found=virus::identity::selectAudioDevice(candidates);
 for(auto candidate:candidates)if(candidate.id==found)model=connectionProfile(candidate.model,snowOnly);
 a={kAudioDevicePropertyStreams,kAudioDevicePropertyScopeInput,kAudioObjectPropertyElementMain};size=0;check(AudioObjectGetPropertyDataSize(found,&a,0,nullptr,&size),"Capture streams");
 if(size!=sizeof(AudioStreamID))throw std::runtime_error("Expected one TI capture stream");AudioStreamID stream=0;check(AudioObjectGetPropertyData(found,&a,0,nullptr,&size,&stream),"Capture stream");
 a={kAudioStreamPropertyVirtualFormat,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};AudioStreamBasicDescription f{};size=sizeof(f);check(AudioObjectGetPropertyData(stream,&a,0,nullptr,&size,&f),"Capture format");
 if(f.mSampleRate!=48000||f.mChannelsPerFrame!=6||f.mFormatID!=kAudioFormatLinearPCM||f.mBitsPerChannel!=32||f.mBytesPerFrame!=24||f.mFormatFlags!=9)throw std::runtime_error("TI audio must be 48 kHz, six-channel float32");
 return found;
}
void checksum(std::vector<uint8_t>& p){unsigned sum=0;for(size_t i=5;i<265;i++)sum+=p[i];p[265]=sum&127;sum=0;for(size_t i=5;i<522;i++)sum+=p[i];p[522]=sum&127;}
}

struct HardwareService::Impl {
 virus::bridge::DeviceBridge& bridge;
 CaptureFIFO capture;
 virus::clock::MarkerDetector marker;
 virus::clock::MarkerTimeline timeline;
 std::atomic<virus::clock::SyncState> syncState{virus::clock::SyncState::inactive};
 // Hardware acceptance must pass before changing this default. The private
 // bench opts in explicitly; ordinary installations keep immediate delivery.
 bool timingEnabled=[] {auto* option=std::getenv("VTR_ENABLE_MARKER_TIMING");return option?std::string(option)=="1":timingDawTestBuild;}();
 PerformanceQueue performance;
 std::atomic<uint64_t> lateEvents{0};
 std::atomic<bool> panicPending{false};
 uint64_t lastWorkerGeneration=0;
 unsigned blockSamples=0,eventOrder=0;
 double blockStart=0,blockStep=1,blockTempo=120,blockBeats=0,nextClock=0;
 bool blockPlaying=false,blockBeatsValid=false,blockCanSend=false,blockLocked=false,clockAnchored=false;
 uint64_t audioGeneration=0;
 uint64_t handledUnderruns=0; // audio consumer only

 USBMeters usbMeters;
 MIDIActivity midiActivity;
 std::array<std::atomic<uint8_t>,16> partVolumes;
 std::atomic<bool> stop{false},live{false},busy{false},hostProcessing{false},outputReady{false};
 std::atomic<uint64_t> submitted{0},completed{0};
 std::array<std::atomic<float>,6> rawPeak;
 std::array<std::atomic<float>,6> renderedPeak;
 std::unique_ptr<LocalControl> control;
 std::string snapshotDirectory;
 std::atomic<int> requested{0};
 std::atomic<bool> leaseHeld{false};
 std::atomic<uint64_t> callbacks{0},invalidCapture{0},notesSent{0};
 std::atomic<uint64_t> stateRevision{0}, stateCaptureCount{0};
 std::atomic<bool> apiRecallDirty{false};
 std::thread worker;
 mutable std::mutex stateLock;
 std::mutex commandLock, snapshotLock, callbackLock;
 std::function<std::vector<uint8_t>()> saveState;
 std::function<bool(const std::vector<uint8_t>&)> restoreState;
 std::atomic<bool> recallPending{false}, reconnectIntent{false};
 bool snapshotFailed=false;
 std::string text="Disconnected · Start with TI in Single mode";
 std::atomic<double> rate{48000};
 std::atomic<unsigned> reportedLatency{2048};
 bool consumerLive=false;
 std::unique_ptr<DeviceSession> midi;
 RestorationGuard restoration;
 void* emulatorLibrary=nullptr;
 std::string emulatorROM;
 bool emulator=false;
 DeviceModel model=DeviceModel::ti16;
 std::atomic<unsigned> supportedParts{16};
 bool performanceSuppressed=false;
 unsigned activeParts()const{return supportedParts.load();}
 unsigned pendingPart=0;
 int mutedPart=-1;
 AudioDeviceID device=0;
 AudioDeviceIOProcID proc=nullptr;
 DeviceLease deviceLock;
 std::atomic<bool> snowSessionConsent{false};
 PatchBytes single,multi,pendingPreset;
 ProjectPatches original,loaded,staged;
 bool lastSucceeded=true;
 std::string presetName;
 struct Library {NSString* path;std::string title;virus::patches::Bank bank;};
 std::vector<Library> library;
 bool libraryReady=false;
 bool dirty=false;
 NSString* directory=nil;
 std::ofstream log;
 explicit Impl(virus::bridge::DeviceBridge& b):bridge(b){
  static_assert(std::atomic<float>::is_always_lock_free,"Metering must remain lock-free");
  for(auto& v:partVolumes)v.store(100);
  for(auto& v:rawPeak)v.store(0);for(auto& v:renderedPeak)v.store(0);capture.setup(rate.load());
  try{control=std::make_unique<LocalControl>([this](const std::string& request){@autoreleasepool{return api(request);}});}
  catch(const std::exception& e){text+=std::string(" · Local API unavailable: ")+e.what();}
 }
 ~Impl(){control.reset();stop=true;if(worker.joinable())worker.join();}
 uint64_t enqueue(int op, const std::vector<uint8_t>& patch={},unsigned target=0){
  if(op<1||op>10||target>=activeParts())return 0;
  std::lock_guard<std::mutex> guard(commandLock);
  if(stop)return 0;
  if(busy.exchange(true))return 0;
  pendingPreset=patch;pendingPart=target;if(op==1)reconnectIntent=true;if(op==10)reconnectIntent=false;if(op==8)recallPending=false;if(op==2){reconnectIntent=false;recallPending=false;}auto id=++submitted;requested=op;
  try{if(!worker.joinable())worker=std::thread([this]{run();});}
  catch(...){requested=0;completed=id;busy=false;throw;}
  return id;
 }
 virus::patches::Bank readBank(NSString* path){
  NSDictionary* attr=[[NSFileManager defaultManager]attributesOfItemAtPath:path error:nil];
  if(![attr[NSFileType]isEqual:NSFileTypeRegular]||[attr[NSFileSize]unsignedLongLongValue]>16*1024*1024)throw std::runtime_error("Expected a regular patch file, at most 16 MiB");
  NSData* data=[NSData dataWithContentsOfFile:path];if(!data)throw std::runtime_error("Cannot read patch file");
  auto* bytes=static_cast<const uint8_t*>(data.bytes);
  return [path.pathExtension.lowercaseString isEqual:@"mid"]?virus::patches::parse_midi_bank(bytes,data.length):virus::patches::parse_sysex_bank(bytes,data.length);
 }
 void prepareLibrary(){
  if(libraryReady)return;
  NSString* root=[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/VirusTIRework/Patches"];
  NSArray* files=[[[NSFileManager defaultManager]enumeratorAtPath:root]allObjects];
  for(NSString* file in [files sortedArrayUsingSelector:@selector(localizedStandardCompare:)])if([file.pathExtension.lowercaseString isEqual:@"mid"]){
   auto path=[root stringByAppendingPathComponent:file];library.push_back({path,file.stringByDeletingPathExtension.UTF8String,readBank(path)});
  }
  libraryReady=true;
 }
 static NSUInteger index(NSDictionary* args,NSString* key,NSUInteger limit){
  id value=args[key];if(![value isKindOfClass:NSNumber.class]||[value doubleValue]<0||[value doubleValue]!=[value unsignedLongLongValue]||[value unsignedLongLongValue]>=limit)throw std::runtime_error("Bank/preset index out of range");return [value unsignedLongLongValue];
 }
 std::string api(const std::string& request){
  NSMutableDictionary* result=[NSMutableDictionary dictionary];
  result[@"protocol"]=@1;result[@"pid"]=@(getpid());result[@"ok"]=@YES;
  result[@"state_revision"]=@(stateRevision.load());result[@"state_capture_count"]=@(stateCaptureCount.load());
  try{
   NSString* command=[NSString stringWithUTF8String:request.c_str()];NSDictionary* args=@{};
   if(!request.empty()&&request.front()=='{'){
    id object=[NSJSONSerialization JSONObjectWithData:[command dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
    if(![object isKindOfClass:NSDictionary.class]||![object[@"command"]isKindOfClass:NSString.class])throw std::runtime_error("Invalid JSON command");args=object;command=args[@"command"];
   }
   int op=[command isEqual:@"connect-snow-test"]?10:[command isEqual:@"connect"]?1:[command isEqual:@"disconnect"]?2:[command isEqual:@"test-tone"]?3:0;
   unsigned target=args[@"part"]?unsigned(index(args,@"part",17)):1;if(target==0||target>activeParts())throw std::runtime_error("Part outside connected device profile");--target;
   std::vector<uint8_t> patch;
   if([command isEqual:@"banks"]||[command isEqual:@"presets"]||[command isEqual:@"load"]){
    prepareLibrary();NSMutableArray* list=[NSMutableArray array];
    if([command isEqual:@"banks"]){for(size_t i=0;i<library.size();i++)[list addObject:@{@"id":@(i),@"name":@(library[i].title.c_str()),@"count":@(library[i].bank.patches.size())}];result[@"banks"]=list;}
    else {auto bank=index(args,@"bank",library.size());auto& patches=library[bank].bank.patches;
     if([command isEqual:@"presets"]){for(size_t i=0;i<patches.size();i++)[list addObject:@{@"index":@(i),@"name":@(patches[i].name.c_str()),@"program":@(patches[i].program),@"bytes":@(patches[i].sysex.size()),@"version":@(patches[i].sysex[9])}];result[@"presets"]=list;}
     else {patch=virus::patches::ram_single(patches[index(args,@"preset",patches.size())],target);op=4;}
    }
   }else if([command isEqual:@"load-file"]){
    if(![args[@"path"]isKindOfClass:NSString.class]||![args[@"path"]isAbsolutePath])throw std::runtime_error("Provide an absolute .mid or .syx path");
    auto bank=readBank(args[@"path"]);patch=virus::patches::ram_single(bank.patches[index(args,@"preset",bank.patches.size())],target);op=4;
   }else if([command isEqual:@"state-save"]||[command isEqual:@"state-load"]){
    if(![args[@"path"]isKindOfClass:NSString.class]||![args[@"path"]isAbsolutePath])throw std::runtime_error("Provide an absolute project-state path");
    std::function<std::vector<uint8_t>()> saver;std::function<bool(const std::vector<uint8_t>&)> loader;
    {std::lock_guard<std::mutex> g(callbackLock);saver=saveState;loader=restoreState;}
    if(!saver||!loader)throw std::runtime_error("Processor state API unavailable");
    if([command isEqual:@"state-save"]){auto bytes=saver();if(bytes.empty())throw std::runtime_error("DAW getState failed; current hardware sound could not be captured");
     if(![[NSData dataWithBytes:bytes.data() length:bytes.size()]writeToFile:args[@"path"] atomically:YES])throw std::runtime_error("Cannot write project state");result[@"state_bytes"]=@(bytes.size());
    }else{NSDictionary* attr=[[NSFileManager defaultManager]attributesOfItemAtPath:args[@"path"] error:nil];
     if(![attr[NSFileType]isEqual:NSFileTypeRegular]||[attr[NSFileSize]unsignedLongLongValue]>65536)throw std::runtime_error("Expected a regular project state file, at most 64 KiB");
     NSData* data=[NSData dataWithContentsOfFile:args[@"path"]];if(!data)throw std::runtime_error("Cannot read project state");auto* bytes=static_cast<const uint8_t*>(data.bytes);
     apiRecallDirty=true;
     if(!loader(std::vector<uint8_t>(bytes,bytes+data.length))){apiRecallDirty=false;throw std::runtime_error("DAW setState rejected invalid state or busy hardware");}
     // Notify after a hardware recall finishes; some hosts immediately ask
     // for state when dirtied, which must not wait through the entire recall.
     if(!recallPending&&!busy&&apiRecallDirty.exchange(false))++stateRevision;
    }
   }else if([command isEqual:@"emulator"]){
    if(live||busy)throw std::runtime_error("Disconnect before selecting an emulator ROM");
    if(![args[@"rom"]isKindOfClass:NSString.class]||![args[@"rom"]isAbsolutePath])throw std::runtime_error("Provide an absolute emulator ROM path");
    std::lock_guard<std::mutex> g(commandLock);emulatorROM=[args[@"rom"]UTF8String];emulator=true;
   }else if([command isEqual:@"note-on"]||[command isEqual:@"note-off"]){
    patch={uint8_t(([command isEqual:@"note-on"]?0x90:0x80)|target),uint8_t(index(args,@"note",128)),uint8_t([command isEqual:@"note-on"]?index(args,@"velocity",128):0)};op=6;
   }else if([command isEqual:@"route"]){
    auto bus=index(args,@"bus",4);if(!bus)throw std::runtime_error("Bus must be 1..3");
    patch={uint8_t(10+(bus-1)*3)};op=9;
   }else if([command isEqual:@"parts"]){
   }else if([command isEqual:@"cutoff"]){
    // One sound control for headless verification of edits made AFTER preset load.
    patch={uint8_t(0xb0|target),40,static_cast<uint8_t>(index(args,@"value",128))};op=6;
   }else if(!op&&![command isEqual:@"status"])throw std::runtime_error("Unknown command; allowed: status, connect, disconnect, test-tone, banks, presets, load, load-file, state-save, state-load, cutoff, emulator, parts, note-on, note-off, route");
   if(op){
    if(op!=2&&!hostProcessing.load())throw std::runtime_error("DAW processor is inactive; enable realtime processing first");
    if(op>=3&&op!=10&&!live.load())throw std::runtime_error("Connect before loading a RAM patch");
    auto id=enqueue(op,patch,target);if(!id)throw std::runtime_error("A hardware command is already pending");result[@"request_id"]=@(id);
   }
  }catch(const std::exception& e){result[@"ok"]=@NO;result[@"error"]=@(e.what());}
  {std::lock_guard<std::mutex> g(stateLock);result[@"status"]=@(text.c_str());result[@"snapshot_directory"]=@(snapshotDirectory.c_str());result[@"preset_name"]=@(presetName.c_str());result[@"last_command_succeeded"]=@(lastSucceeded);}
  result[@"timing_enabled"]=@(timingEnabled);result[@"timing_status"]=@(virus::clock::syncName(syncState.load()));result[@"timing_marker_frame"]=@(marker.validMarker());result[@"timing_overflow"]=@(performance.overflow.load());result[@"timing_late_events"]=@(lateEvents.load());result[@"performance_dropped"]=@(performance.dropped.load());result[@"automation_coalesced"]=@(performance.coalesced.load());
  result[@"device_lease_held"]=@(leaseHeld.load());result[@"connected"]=@(live.load());result[@"busy"]=@(busy.load());result[@"completed_request_id"]=@(completed.load());
  result[@"state_recall_pending"]=@(recallPending.load());result[@"state_format"]=@3;result[@"backend"]=emulator?@"emulator":@"hardware";
  {std::lock_guard<std::mutex> g(stateLock);NSMutableArray* parts=[NSMutableArray array];for(unsigned p=0;p<16;++p){auto& bytes=staged.parts[p].empty()?loaded.parts[p]:staged.parts[p];[parts addObject:@{@"part":@(p+1),@"name":bytes.empty()?@"":@(virus::patches::parse_single(bytes).name.c_str()),@"bytes":@(bytes.size())}];}result[@"parts"]=parts;}
  result[@"reported_latency_samples"]=@(reportedLatency.load());result[@"timing_daw_test_build"]=@(timingDawTestBuild);
  result[@"host_processing"]=@(hostProcessing.load());result[@"host_sample_rate"]=@(rate.load());result[@"device_sample_rate"]=@48000;
  result[@"firmware_operations_supported"]=@NO;result[@"stored_bank_writes_supported"]=@NO;result[@"active_part_count"]=@(activeParts());result[@"snow_hardware_verified"]=@NO;result[@"snow_test_build"]=@(snowPlaybackTestBuild);result[@"snow_test_consent"]=@(snowSessionConsent.load());
  result[@"lifetime_counters"]=@{@"capture_callbacks":@(callbacks.load()),@"capture_errors":@(invalidCapture.load()),@"fifo_overruns":@(capture.overruns.load()),@"fifo_underruns":@(capture.underruns.load()),@"rendered_frames":@(capture.rendered.load()),@"midi_sent":@(notesSent.load())};
  NSMutableArray* raw=[NSMutableArray array];NSMutableArray* rendered=[NSMutableArray array];
  for(unsigned c=0;c<6;c++){[raw addObject:@(rawPeak[c].load())];[rendered addObject:@(renderedPeak[c].load())];}
  NSMutableArray* meterValues=[NSMutableArray array];auto peaks=usbMeters.read(live.load()&&outputReady.load());if(activeParts()==4)peaks[2]=0;for(float level:peaks)[meterValues addObject:@(level)];result[@"usb_peak_meters"]=meterValues;result[@"part_meter_source"]=@"midi-note-on-velocity";NSMutableArray* activity=[NSMutableArray array];for(float level:midiActivity.read(live.load()))[activity addObject:@(level)];result[@"part_midi_activity"]=activity;
  result[@"lifetime_raw_channel_peaks"]=raw;result[@"lifetime_plugin_channel_peaks"]=rendered;
  NSData* json=[NSJSONSerialization dataWithJSONObject:result options:0 error:nil];return std::string(static_cast<const char*>(json.bytes),json.length);
 }
 void sendMidi(const PatchBytes& bytes){midi->send(bytes);midiActivity.midi(bytes.data(),bytes.size());if(bytes.size()==3&&(bytes[0]&0xf0)==0xb0&&bytes[1]==7)partVolumes[bytes[0]&15]=bytes[2];}
 void replayVolumes(){for(unsigned p=0;p<activeParts();++p)sendMidi({uint8_t(0xb0|p),7,partVolumes[p].load()});}
 void status(std::string message){if(activeParts()==4)message="Snow hardware unverified · "+message;std::lock_guard<std::mutex> g(stateLock);text=std::move(message);if(log)log<<text<<std::endl;}
 static OSStatus audio(AudioDeviceID,const AudioTimeStamp*,const AudioBufferList* input,const AudioTimeStamp*,AudioBufferList*,const AudioTimeStamp*,void* ref){
  auto& s=*static_cast<Impl*>(ref);++s.callbacks;
  // This is capture-only. Do not write the DAW's audio output buffers.
  if(!input||input->mNumberBuffers!=1||input->mBuffers[0].mNumberChannels!=6||!input->mBuffers[0].mData||input->mBuffers[0].mDataByteSize%24){++s.invalidCapture;s.marker.disarm();if(s.syncState.exchange(virus::clock::SyncState::degraded)==virus::clock::SyncState::locked)s.panicPending=true;return noErr;}
  auto* samples=static_cast<const float*>(input->mBuffers[0].mData);auto frames=input->mBuffers[0].mDataByteSize/24;
  for(unsigned c=0;c<6;c++){float peak=s.rawPeak[c].load(std::memory_order_relaxed);for(unsigned f=0;f<frames;f++)if(std::isfinite(samples[f*6+c]))peak=std::max(peak,std::abs(samples[f*6+c]));s.rawPeak[c].store(peak,std::memory_order_relaxed);}
  auto first=s.capture.newest();
  if(s.capture.push(samples,frames)){s.marker.feed(samples,frames,first);auto found=s.marker.validMarker();if(found>=0)s.capture.suppressCalibrationMarker(found);}
  else {s.marker.disarm();s.syncState=virus::clock::SyncState::degraded;s.panicPending=true;}
  s.usbMeters.push(samples,frames);return noErr;
 }
 void save(NSString* name,const std::vector<uint8_t>& bytes){if(![[NSData dataWithBytes:bytes.data() length:bytes.size()]writeToFile:[directory stringByAppendingPathComponent:name] atomically:YES])throw std::runtime_error("Cannot save hardware snapshot");}
 void pause(unsigned milliseconds){auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);do{if(live)drainMidi();std::this_thread::sleep_for(std::chrono::milliseconds(2));}while(std::chrono::steady_clock::now()<end);}
 static void emulatorAudio(void* context,const float* samples,unsigned frames){auto& self=*static_cast<Impl*>(context);if(!self.live)return;++self.callbacks;self.usbMeters.push(samples,frames);self.capture.push(samples,frames);for(unsigned c=0;c<6;++c){float peak=self.rawPeak[c];for(unsigned f=0;f<frames;++f)peak=std::max(peak,std::abs(samples[f*6+c]));self.rawPeak[c]=peak;}}
 void connect(bool snowOnly=false){
  snowSessionConsent=false;
  outputReady=false;performanceSuppressed=true;syncState=virus::clock::SyncState::preparing;
  status(emulator?"Booting OsTIrus TI emulator…":"Connecting TI · preserving original edit buffers…");
  NSString* base=[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/VirusTIRework"];
  NSError* error=nil;if(![[NSFileManager defaultManager]createDirectoryAtPath:base withIntermediateDirectories:YES attributes:nil error:&error])throw std::runtime_error("Cannot create snapshot directory");
  if(!emulator){auto path=[base stringByAppendingPathComponent:@"hardware.lock"];deviceLock.acquire(path.fileSystemRepresentation);leaseHeld=true;}
  directory=[base stringByAppendingPathComponent:NSUUID.UUID.UUIDString];{std::lock_guard<std::mutex> g(stateLock);snapshotDirectory=directory.fileSystemRepresentation;}
  if(![[NSFileManager defaultManager]createDirectoryAtPath:directory withIntermediateDirectories:NO attributes:nil error:&error])throw std::runtime_error("Cannot create session snapshot directory");
  log.open([[directory stringByAppendingPathComponent:@"session.log"]fileSystemRepresentation]);
  if(emulator){
   auto path=[resources() stringByAppendingPathComponent:@"Emulator/libosti-backend.dylib"];
   emulatorLibrary=dlopen(path.fileSystemRepresentation,RTLD_NOW|RTLD_LOCAL);if(!emulatorLibrary)throw std::runtime_error(std::string("Emulator runtime unavailable: ")+dlerror());
   auto create=reinterpret_cast<CreateEmulator>(dlsym(emulatorLibrary,"vtrCreateEmulator"));if(!create)throw std::runtime_error("Emulator runtime entrypoint absent");
   char error[1024]{};midi.reset(create(emulatorROM.c_str(),emulatorAudio,this,error,sizeof(error)));if(!midi)throw std::runtime_error(error);
  }else{device=findDevice(model,snowOnly);supportedParts=partCount(model);midi=std::make_unique<HardwareMidi>(model==DeviceModel::snow4);}
  // The ordinary Connect TI action is sufficient to start a Snow session.
  // Preserve its record for the helper and enforce all Snow preflight checks.
  snowSessionConsent=model==DeviceModel::snow4;
  supportedParts=partCount(model);
  {std::lock_guard<std::mutex> g(stateLock);if(model==DeviceModel::snow4)normalizeSnowPlaybackProject(staged);else normalizeProject(staged,model);} // Reject cross-model projects before device commands.
  auto before=captureSnapshot(*midi,model,[&](std::string name,const auto& bytes){save(@(name.c_str()),bytes);},[&]{if(stop)throw std::runtime_error("Shutdown during snapshot");},snowSessionConsent.load());
  original=before.project;single=before.single;multi=before.project.multi;
  if(!emulator){auto helper=[resources() stringByAppendingPathComponent:@"Helpers/virus-ti-restoration-helper"];restoration.arm(helper.fileSystemRepresentation,directory.fileSystemRepresentation,static_cast<HardwareMidi*>(midi.get())->registryID(),deviceLock.descriptor());}
  if(stop)throw std::runtime_error("Shutdown before connection changes");
  if(!emulator){
   check(AudioDeviceCreateIOProcID(device,audio,this,&proc),"Create TI capture callback");
   struct Usage {AudioDeviceIOProcID io;UInt32 count;UInt32 on[1];} usage{proc,1,{0}};
   AudioObjectPropertyAddress property{kAudioDevicePropertyIOProcStreamUsage,kAudioDevicePropertyScopeOutput,kAudioObjectPropertyElementMain};
   check(AudioObjectSetPropertyData(device,&property,0,nullptr,sizeof(usage),&usage),"Set capture-only stream usage");
  }
  capture.setTarget(emulator?8192:timingEnabled&&model==DeviceModel::ti16?timeline.latencyDevice():2048);
  dirty=true;sendMidi({0xf0,0,0x20,0x33,1,16,0x73,0,122,uint8_t(model==DeviceModel::snow4?1:3),0xf7});pause(350);
  // Original Global USB / Virus Control USB IO Configuration: 1 = three
  // synth returns, no input. CoreAudio's first pair otherwise carries input.
  // This is a host-selected configuration, not part of a sound snapshot.
  if(!emulator&&model==DeviceModel::ti16){sendMidi({0xf0,0,0x20,0x33,1,16,0x73,0,9,1,0xf7});pause(100);}
  ProjectPatches desired;{std::lock_guard<std::mutex> g(stateLock);desired=staged;}
  auto routing=desired.multi.empty()?multi:desired.multi;
  if(desired.multi.empty())for(unsigned p=0;p<activeParts();++p){routing[73+p]=p;routing[89+p]=0;routing[105+p]=127;routing[153+p]=127;routing[169+p]=127;routing[185+p]=10;routing[249+p]|=1;}
  multiChecksum(routing);sendMidi(routing);pause(multiSettleMs);
  auto actualMulti=midi->multi();if(!sameMulti(actualMulti,routing,model))throw std::runtime_error("Initial multi sound/routing mismatch");{std::lock_guard<std::mutex> g(stateLock);loaded.multi=actualMulti;}
  const auto callbacksBeforeStart=callbacks.load(),framesBeforeStart=capture.newest();
  if(!emulator)check(AudioDeviceStart(device,proc),"Start TI capture");live=true;
  for(unsigned p=0;p<activeParts();++p){auto source=desired.parts[p].empty()?(p==0?single:original.parts[p]):desired.parts[p];load(source,p);}
  if(!emulator&&(callbacks.load()==callbacksBeforeStart||capture.newest()==framesBeforeStart))throw std::runtime_error("No TI capture callbacks; check host microphone permission and audio availability");
  replayVolumes();{std::lock_guard<std::mutex> g(stateLock);staged={};}
  // The TI can emit an initialization transient while routing/edit buffers settle.
  // Keep the host silent until preparation completes; beginBlock then re-primes.
  usbMeters.clear();drainMidi();calibrateClock();performanceSuppressed=false;outputReady=true;
  status((emulator?"Emulator connected · ":"Connected · ")+std::to_string(activeParts())+" parts");
 }
 PatchBytes presetReadback(unsigned part,bool serviceMidi=false){
  return midi->single(part,serviceMidi?std::function<void()>([this]{drainMidi();}):std::function<void()>{});
 }
 void load(const PatchBytes& source,unsigned part){
  if(stop)throw std::runtime_error("Shutdown during preset recall");
  if(!midi||!live||part>=activeParts())throw std::runtime_error("Connect before loading a RAM preset");
  auto patch=virus::patches::ram_single(virus::patches::parse_single(source),part);auto label=virus::patches::parse_single(patch).name;
  status("Loading RAM part "+std::to_string(part+1)+" · "+label);
  struct Mute {int& value;~Mute(){value=-1;}} mute{mutedPart};mutedPart=int(part);
  sendMidi({uint8_t(0xb0|part),64,0});sendMidi({uint8_t(0xb0|part),123,0});drainMidi();
  save([NSString stringWithFormat:@"preset-request-part%u.syx",part],patch);sendMidi(patch);pause(300);
  auto actual=presetReadback(part,true);save([NSString stringWithFormat:@"preset-readback-part%u.syx",part],actual);
  if(model==DeviceModel::snow4&&actual!=patch)throw std::runtime_error("Snow preset readback mismatch");
  auto parsed=virus::patches::parse_single(actual);if(parsed.name!=label||actual[8]!=part)throw std::runtime_error("Preset part/name readback mismatch");
  if(presetReadback(part,true)!=actual)throw std::runtime_error("Preset readback unstable");
  if(patch[9]>=12&&actual.size()!=patch.size())throw std::runtime_error("Current-version preset size mismatch");
  for(size_t i=9;i<patch.size()-2;++i)if(i!=265&&i!=10&&patch[i]!=actual[i]&&patch[9]>=12)throw std::runtime_error("Current-version preset sound-byte mismatch");
  {std::lock_guard<std::mutex> g(stateLock);loaded.parts[part]=actual;snapshotFailed=false;presetName=label;}
  status("Connected · Part "+std::to_string(part+1)+" · "+label+" · RAM preset verified");
 }
 void testTone(unsigned part){auto path=[resources() stringByAppendingPathComponent:@"Hardware/stock-wavetable-probe.syx"];NSData* data=[NSData dataWithContentsOfFile:path];if(data.length!=524)throw std::runtime_error("Stock test patch resource absent");auto* p=static_cast<const uint8_t*>(data.bytes);load(PatchBytes(p,p+524),part);}
 void route(unsigned part,uint8_t output){requirePart(model,part);sendMidi({0xf0,0,0x20,0x33,1,16,0x72,uint8_t(part),41,output,0xf7});auto current=midi->multi();validateMulti(current);if(current[185+part]!=output)throw std::runtime_error("Part routing verification failed");std::lock_guard<std::mutex> g(stateLock);loaded.multi=current;}
 void calibrateClock(){
  using namespace virus::clock;
  marker.disarm();
  if(!timingEnabled||emulator||model==DeviceModel::snow4){syncState=SyncState::degraded;return;}
  syncState=SyncState::calibrating;
  for(unsigned attempt=0;attempt<3&&!stop;++attempt){
   auto quietDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(6);
   while(!stop&&capture.newest()-std::min(capture.newest(),marker.lastSound.load())<48000){
    if(std::chrono::steady_clock::now()>=quietDeadline){syncState=SyncState::degraded;return;}pause(10);
   }
   if(stop){syncState=SyncState::inactive;return;}
   marker.arm();const auto newest=capture.newest();
   auto initial=uint16_t((newest-5120)&16383);
   midi->sendTimed(clockMessage(ClockCommand::activeSense),initial,true);
   midi->sendTimed(clockMessage(ClockCommand::songPosition),initial,true);
   midi->sendTimed(clockMessage(ClockCommand::calibrationPing),3072,true);
   const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
   while(!stop&&std::chrono::steady_clock::now()<deadline){
    if(marker.validMarker()>=0){syncState=SyncState::locked;return;}pause(10);
   }
   marker.disarm();
  }
  syncState=SyncState::degraded;
 }
 void drainPerformance(){
  auto generation=bridge.generation();
  bool changed=lastWorkerGeneration&&lastWorkerGeneration!=generation;lastWorkerGeneration=generation;
  bool unsafe=panicPending.exchange(false)|performance.panic.exchange(false);if(unsafe)bridge.serviceDisconnect();
  if((changed||unsafe)&&midi&&live){for(unsigned p=0;p<activeParts();++p){sendMidi({uint8_t(0xb0|p),64,0});sendMidi({uint8_t(0xb0|p),123,0});}}
  std::array<PerformanceEvent,4096> ready{};auto count=performance.take(ready.data(),ready.size());
  std::stable_sort(ready.begin(),ready.begin()+count,PerformanceQueue::before);
  std::vector<TimedMessage> batch;batch.reserve(32);
  auto flush=[&]{if(!batch.empty()){midi->sendTimedBatch(batch);batch.clear();}};
  for(unsigned i=0;i<count;++i){const auto& e=ready[i];
   if(unsafe||performanceSuppressed||(!e.clock&&(e.bytes[0]&15)>=activeParts())||!bridge.ready()||!live||!outputReady||!hostProcessing||e.generation!=generation||int(e.bytes[0]&15)==mutedPart){++performance.dropped;continue;}
   PatchBytes bytes=e.clock?(e.bytes[0]==0xf2?PatchBytes(e.bytes.begin(),e.bytes.end()):PatchBytes{e.bytes[0]}):PatchBytes(e.bytes.begin(),e.bytes.end());
   if(e.frame>=0&&syncState.load()==virus::clock::SyncState::locked){uint16_t stamp=0;
    if(virus::clock::MarkerTimeline::stamp(e.frame,marker.validMarker(),capture.newest(),stamp)){batch.push_back({bytes,stamp,e.clock});if(batch.size()==32)flush();}
    else{++lateEvents;if(!e.clock){auto type=e.bytes[0]&0xf0;if(type==0x80||(type==0x90&&e.bytes[2]==0)){flush();sendMidi(bytes);}else ++performance.dropped;}continue;}
   }else if(!e.clock){flush();sendMidi(bytes);}
   if(!e.clock){++notesSent;midiActivity.midi(bytes.data(),bytes.size());if((bytes[0]&0xf0)==0xb0&&bytes[1]==7)partVolumes[bytes[0]&15]=bytes[2];}
  }
  flush();
 }
 void drainMidi(){
  drainPerformance();
  if(!outputReady.load())return; // discard this generation at transaction boundaries
  virus::bridge::MidiMessage event;unsigned budget=2048;
  while(budget--&&bridge.serviceTakeMidi(event)){
   auto statusByte=event.bytes[0]&0xf0;
   if(!performanceSuppressed&&(event.bytes[0]&15)<activeParts()&&int(event.bytes[0]&15)!=mutedPart&&(statusByte==0x80||statusByte==0x90||statusByte==0xb0)){sendMidi({event.bytes.begin(),event.bytes.end()});++notesSent;if(log)log<<"MIDI "<<unsigned(event.bytes[0])<<" "<<unsigned(event.bytes[1])<<" "<<unsigned(event.bytes[2])<<" frame="<<event.block_start+event.sample_offset<<" sent_ns="<<std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()<<std::endl;}
  }
 }
 void snapshot(){
  if(!live)return;
  try{ProjectPatches current;drainMidi();
   // Read each edit buffer once, with both protocol checksums verified by
   // single(). The TI cannot queue overlapping dump requests. Repeating all
   // sixteen reads exceeded Bitwig's state-save deadline (~33 seconds).
   // Publish only after every fresh reply and the multi have validated.
   for(unsigned p=0;p<activeParts();++p){if(stop)throw std::runtime_error("Host closing");current.parts[p]=presetReadback(p,true);}
   for(unsigned p=0;p<activeParts();++p){virus::patches::parse_single(current.parts[p]);save([NSString stringWithFormat:@"project-part%u.syx",p],current.parts[p]);}
   current.multi=midi->multi();validateMulti(current.multi);save(@"project-multi.syx",current.multi);
   std::lock_guard<std::mutex> g(stateLock);loaded=std::move(current);snapshotFailed=false;
  }catch(...){std::lock_guard<std::mutex> g(stateLock);snapshotFailed=true;throw;}
 }
 void disconnect(bool closing=false){
  const bool hadHardwareState = midi && dirty;
  std::string errors;auto attempt=[&](auto action){try{action();}catch(const std::exception& e){errors+=std::string(e.what())+"; ";}};
  if(live&&!closing)attempt([&]{snapshot();std::lock_guard<std::mutex> g(stateLock);if(staged.empty())staged=loaded;});
  closing=closing||stop.load();
  outputReady=false;marker.disarm();syncState=virus::clock::SyncState::inactive;if(live.exchange(false))bridge.serviceDisconnect();
  if(proc){attempt([&]{check(AudioDeviceStop(device,proc),"Stop TI capture");});attempt([&]{check(AudioDeviceDestroyIOProcID(device,proc),"Destroy TI capture");});proc=nullptr;}
  if(closing&&restoration.armed()){
   if(midi&&directory)attempt([&]{midi->saveTraffic(directory.fileSystemRepresentation);});midi.reset();dirty=false;
   // The helper retains the shared lease. Closing our descriptor must NOT
   // explicitly unlock it; no new owner may enter before readback completes.
   deviceLock.close();leaseHeld=false;
   restoration.handoff();usbMeters.clear();midiActivity.clear();status("Host closing · automatic RAM restoration in progress");log.close();return;
  }
  if(midi&&dirty){
   RestoreSnapshot recovery;recovery.model=model;recovery.snowTestConsent=snowSessionConsent;recovery.project=original;recovery.single=single;
   attempt([&]{restoreSnapshot(*midi,recovery,[&](std::string name,const auto& bytes){save(@(name.c_str()),bytes);},[&](auto ms){pause(ms);},!emulator&&model==DeviceModel::ti16);});
  }
  usbMeters.clear();midiActivity.clear();
  performanceSuppressed=false;dirty=false;if(midi&&directory)attempt([&]{midi->saveTraffic(directory.fileSystemRepresentation);});midi.reset();if(emulatorLibrary){dlclose(emulatorLibrary);emulatorLibrary=nullptr;}
  if(restoration.armed()){if(errors.empty())attempt([&]{restoration.complete();});else restoration.handoff();}
  deviceLock.close();leaseHeld=false;
  status(errors.empty()?(hadHardwareState?("Disconnected · All "+std::to_string(activeParts())+" parts restored · Single mode sent"):"Disconnected · No device state changed"):"Disconnected · Restore needs attention: "+errors);log.close();
 }
 void run(){@autoreleasepool{
  while(!stop){@autoreleasepool{int op=requested.exchange(0);
   if(!op&&recallPending&&hostProcessing){std::lock_guard<std::mutex> g(commandLock);if(!busy.exchange(true)){op=7;++submitted;recallPending=false;}}
   try{
   if((op==1||op==10)&&!live)connect(op==10);else if(op==2||op==8)disconnect(stop.load());else if(op==3)testTone(pendingPart);else if(op==4)load(pendingPreset,pendingPart);
   else if(op==5)snapshot();else if(op==6){
    uint16_t stamp=0;auto target=double(capture.newest())+virus::clock::MarkerTimeline::transmitLead;
    if(syncState.load()==virus::clock::SyncState::locked&&virus::clock::MarkerTimeline::stamp(target,marker.validMarker(),capture.newest(),stamp))midi->sendTimed(pendingPreset,stamp,false);else sendMidi(pendingPreset);++notesSent;
   }
   else if(op==9)route(pendingPart,pendingPreset.at(0));
   else if(op==7){if(!live)connect();else{
    ProjectPatches desired;{std::lock_guard<std::mutex> g(stateLock);desired=staged;}
    if(model==DeviceModel::snow4)desired=normalizeSnowPlaybackProject(desired);
    performanceSuppressed=true;outputReady=false;bridge.serviceDisconnect();
    auto recalled=recallProject(*midi,desired,model,[&](auto ms){pause(ms);},[&](const auto& bytes,unsigned part){load(bytes,part);std::lock_guard<std::mutex> g(stateLock);return loaded.parts[part];});
    replayVolumes();drainMidi();
    {std::lock_guard<std::mutex> g(stateLock);if(!recalled.multi.empty())loaded.multi=std::move(recalled.multi);staged={};}
    calibrateClock();performanceSuppressed=false;outputReady=true;
   }}

   if(op){std::lock_guard<std::mutex> g(stateLock);lastSucceeded=text.find("needs attention")==std::string::npos;}
   if(op==1||op==10||op==2||op==3||op==4||op==9||(op==6&&!pendingPreset.empty()&&(pendingPreset[0]&0xf0)==0xb0))++stateRevision;
   if(op==7&&apiRecallDirty.exchange(false))++stateRevision;
   if(live)drainMidi();
  }catch(const std::exception& e){std::string cause=e.what();if(op!=5){disconnect(stop.load());syncState=virus::clock::SyncState::error;}std::string restored;{std::lock_guard<std::mutex> g(stateLock);restored=text;}status(std::string(op==5?"Project save failed: ":"Connection failed: ")+cause+" · "+restored);{std::lock_guard<std::mutex> g(stateLock);lastSucceeded=false;}}
   if(op){completed=submitted.load();busy=false;}
  }std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  disconnect(true);
 }}
};
HardwareService::HardwareService(virus::bridge::DeviceBridge& b):impl(std::make_unique<Impl>(b)){}
HardwareService::~HardwareService()=default;
void HardwareService::shutdownControl(){impl->control.reset();}
void HardwareService::shutdown(){impl->stop=true;impl->recallPending=false;impl->control.reset();if(impl->worker.joinable())impl->worker.join();}
void HardwareService::command(int op){impl->enqueue(op);}
bool HardwareService::loadPreset(const PatchBytes& patch,unsigned part){
 try{if(part>=impl->activeParts())throw std::runtime_error("Part outside connected device profile");auto ram=virus::patches::ram_single(virus::patches::parse_single(patch),part);if(!impl->live){std::lock_guard<std::mutex> guard(impl->commandLock);if(impl->busy)return false;{std::lock_guard<std::mutex> g(impl->stateLock);impl->staged.parts[part]=ram;}++impl->stateRevision;impl->status("Preset staged · Part "+std::to_string(part+1));return true;}return impl->hostProcessing&&impl->enqueue(4,ram,part);}catch(const std::exception& e){impl->status(std::string("Preset rejected: ")+e.what());return false;}
}
PatchBytes HardwareService::currentPreset(unsigned part)const{if(part>=16)return {};std::lock_guard<std::mutex> g(impl->stateLock);return impl->staged.parts[part].empty()?impl->loaded.parts[part]:impl->staged.parts[part];}
void HardwareService::stateCallbacks(std::function<std::vector<uint8_t>()> save,std::function<bool(const std::vector<uint8_t>&)> restore){std::lock_guard<std::mutex> g(impl->callbackLock);impl->saveState=std::move(save);impl->restoreState=std::move(restore);}
bool HardwareService::captureProject(ProjectPatches& patch,bool& reconnect){
 ++impl->stateCaptureCount;
 if(impl->stop)return false;
 std::lock_guard<std::mutex> serial(impl->snapshotLock);
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
 while(impl->busy||(impl->recallPending&&impl->hostProcessing)){if(impl->stop||std::chrono::steady_clock::now()>deadline)return false;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
 if(impl->live){auto id=impl->enqueue(5);if(!id)return false;
  while(impl->completed<id){if(impl->stop||std::chrono::steady_clock::now()>deadline)return false;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
  std::lock_guard<std::mutex> g(impl->stateLock);if(!impl->lastSucceeded)return false;
 }
 std::lock_guard<std::mutex> g(impl->stateLock);if(impl->snapshotFailed&&impl->staged.empty())return false;
 patch=impl->loaded;for(unsigned p=0;p<16;++p)if(!impl->staged.parts[p].empty())patch.parts[p]=impl->staged.parts[p];if(!impl->staged.multi.empty())patch.multi=impl->staged.multi;reconnect=impl->reconnectIntent&&impl->activeParts()!=4;return true;
}
bool HardwareService::restoreProject(const ProjectPatches& patch,bool reconnect,const std::array<uint8_t,16>* volumes){
 try{
  auto ram=impl->live&&impl->activeParts()==4?normalizeSnowPlaybackProject(patch):normalizeProject(patch);
  std::lock_guard<std::mutex> snapshotGuard(impl->snapshotLock);
  std::lock_guard<std::mutex> commandGuard(impl->commandLock);if(impl->busy||impl->stop)return false;
  if(volumes)for(unsigned p=0;p<16;++p)impl->partVolumes[p]=(*volumes)[p];
  // Start the dormant worker here, never from the audio callback.
  if(!ram.empty()&&!impl->worker.joinable())impl->worker=std::thread([p=impl.get()]{p->run();});
  {std::lock_guard<std::mutex> g(impl->stateLock);impl->staged=ram;impl->loaded=ram;impl->snapshotFailed=false;impl->lastSucceeded=true;impl->presetName=ram.parts[0].empty()?"":virus::patches::parse_single(ram.parts[0]).name;}
  impl->reconnectIntent=reconnect;impl->recallPending=!ram.empty()&&(reconnect||impl->live);
  impl->status(ram.empty()?"Legacy project controls restored":impl->recallPending?"Project sound staged · waiting for realtime recall":"Project sound staged · Connect TI to recall");return true;
 }catch(...){return false;}
}
std::string HardwareService::controlEndpoint()const{return impl->control?impl->control->path():std::string{};}
MeterSnapshot HardwareService::meters()const noexcept{MeterSnapshot result;result.usbPeak=impl->usbMeters.read(impl->live.load()&&impl->outputReady.load());if(impl->activeParts()==4)result.usbPeak[2]=0;result.midiActivity=impl->midiActivity.read(impl->live.load());return result;}
std::string HardwareService::status()const{std::lock_guard<std::mutex> g(impl->stateLock);return impl->text+(impl->live.load()?std::string(" · Timing ")+(impl->syncState.load()==virus::clock::SyncState::locked?"locked":"unsynchronized"):"");}
void HardwareService::setup(double rate,unsigned maxBlock){impl->rate=rate;impl->capture.setup(rate);impl->timeline.setup(rate,maxBlock);impl->reportedLatency=impl->timingEnabled?impl->timeline.latencyHost():impl->capture.latency(rate);}
void HardwareService::beginBlock(bool active,unsigned samples,double tempo,double beats,bool playing,bool beatsValid)noexcept{
 auto& s=*impl;s.hostProcessing.store(active);s.blockSamples=samples;s.blockTempo=std::isfinite(tempo)&&tempo>0?std::clamp(tempo,1.,1000.):120;s.blockBeats=beats;s.blockPlaying=playing;s.blockBeatsValid=beatsValid&&std::isfinite(beats);
 bool live=s.live.load(std::memory_order_acquire)&&s.outputReady.load(std::memory_order_acquire)&&active;
 if(live!=s.consumerLive){s.capture.resetConsumer();s.consumerLive=live;s.clockAnchored=false;s.timeline.valid=false;}
 auto generation=s.bridge.generation();
 if(generation!=s.audioGeneration){s.capture.resetConsumer();s.audioGeneration=generation;s.clockAnchored=false;s.timeline.valid=false;}
 if(live)s.bridge.serviceReady(generation);
 s.blockLocked=live&&s.syncState.load()==virus::clock::SyncState::locked;
 if(s.blockLocked)s.capture.beginAlignedBlock(s.timeline.begin(s.capture.newest(),generation));else s.capture.beginBlock(live);
 s.blockCanSend=live&&(!s.blockLocked||s.capture.ready());
 s.blockStep=s.capture.increment();s.blockStart=s.capture.position()+s.timeline.latencyDevice();
 s.performance.begin(generation,s.blockCanSend,s.blockLocked,s.blockStart,s.blockStep,samples,s.rate.load());
}
bool HardwareService::queueMidi(unsigned offset,std::array<uint8_t,3> bytes)noexcept{
 if((bytes[0]&15)>=impl->activeParts())return false;
 return impl->performance.submit(offset,bytes);
}
void HardwareService::endBlock()noexcept{
 auto& s=*impl;

 if(s.blockLocked&&s.blockCanSend&&s.blockSamples){
  if(!s.clockAnchored){s.performance.publish(0,{0xfe,0,0},true,0,-1024);unsigned spp=s.blockBeatsValid?unsigned(std::fmod(std::max(0.,s.blockBeats)*4,16384.)):0;s.performance.publish(0,{0xf2,uint8_t(spp&127),uint8_t((spp>>7)&127)},true,0,-1024);s.nextClock=s.blockStart;s.clockAnchored=true;}
  const double tick=48000.*60/(s.blockTempo*24);unsigned emitted=0;
  while(s.nextClock<s.blockStart+s.blockSamples*s.blockStep&&emitted<256){++emitted;if(s.nextClock>=s.blockStart){auto offset=unsigned(std::llround((s.nextClock-s.blockStart)/s.blockStep));s.performance.publish(std::min(offset,s.blockSamples-1),{0xf8,0,0},true,0);}s.nextClock+=tick;}
  if(emitted==256)s.nextClock=s.blockStart+s.blockSamples*s.blockStep;
  s.timeline.advance(s.blockSamples);
 }
 s.performance.end();
}
bool HardwareService::render(std::array<int32_t,6>& value)noexcept{
 if(!impl->consumerLive)return false;
 if(!impl->capture.pop(value)){
  const auto count=impl->capture.underruns.load();
  if(count!=impl->handledUnderruns){
   impl->handledUnderruns=count;
   // A confirmed marker cannot justify synchronized playback after a capture
   // gap. Cancel the old epoch on the worker; never reping under held notes.
   if(impl->timingEnabled){impl->marker.disarm();impl->syncState=virus::clock::SyncState::degraded;impl->panicPending=true;impl->blockLocked=false;impl->blockCanSend=false;}
  }
  return false;
 }
 if(impl->activeParts()==4){value[4]=0;value[5]=0;}
 for(unsigned c=0;c<6;c++){float peak=float(std::abs(double(value[c])/2147483648.));if(peak>impl->renderedPeak[c].load(std::memory_order_relaxed))impl->renderedPeak[c].store(peak,std::memory_order_relaxed);}
 return true;
}
void HardwareService::hostProcessing(bool active)noexcept{impl->hostProcessing.store(active);}
bool HardwareService::connected()const noexcept{return impl->live.load(std::memory_order_acquire);}
uint64_t HardwareService::stateRevision()const noexcept{return impl->stateRevision.load();}
unsigned HardwareService::latency()const noexcept{return impl->reportedLatency.load();}
#ifdef VIRUS_REWORK_TEST
bool HardwareService::testCaptureUnderrunFallback(){
 virus::bridge::DeviceBridge bridge;HardwareService service(bridge);auto& s=*service.impl;
 bridge.restart(true);bridge.serviceReady(bridge.generation());
 s.timingEnabled=true;s.syncState=virus::clock::SyncState::locked;
 s.capture.setup(48000);std::array<float,1024*6> silence{};
 if(!s.capture.push(silence.data(),1024))return false;
 s.capture.beginAlignedBlock(900);s.consumerLive=true;s.blockLocked=true;s.blockCanSend=true;
 std::array<int32_t,6> value{};for(unsigned i=0;i<256;++i)service.render(value);
 const bool degraded=s.capture.underruns==1&&s.syncState==virus::clock::SyncState::degraded&&s.panicPending&&!s.blockLocked&&!s.blockCanSend;
 auto previous=bridge.generation();s.drainPerformance(); // control-thread cancellation path
 const bool blocked=!bridge.ready();bridge.beginBlock(); // processor consumes the epoch change
 s.consumerLive=false;
 return degraded&&blocked&&bridge.generation()!=previous&&!s.panicPending&&!s.leaseHeld&&!s.live&&s.callbacks==0&&s.notesSent==0;
}
#endif
unsigned HardwareService::activePartCount()const noexcept{return impl->activeParts();}
}
