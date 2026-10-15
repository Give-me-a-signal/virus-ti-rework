#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#import <CoreMIDI/CoreMIDI.h>
#include <atomic>
#include <array>
#include <chrono>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include "plugin_host.hpp"
#include "../private-midi/hardware_midi.hpp"

static void check(OSStatus s,const char* op){if(s)throw std::runtime_error(std::string(op)+": "+std::to_string(s));}
static std::string midiName(MIDIObjectRef o){CFStringRef s=nullptr;MIDIObjectGetStringProperty(o,kMIDIPropertyDisplayName,&s);if(!s)return {};std::string r=[(__bridge NSString*)s UTF8String];CFRelease(s);return r;}
static std::string audioName(AudioDeviceID d){CFStringRef s=nullptr;UInt32 z=sizeof(s);AudioObjectPropertyAddress a{kAudioObjectPropertyName,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};check(AudioObjectGetPropertyData(d,&a,0,nullptr,&z,&s),"audio name");std::string r=[(__bridge NSString*)s UTF8String];CFRelease(s);return r;}
static AudioDeviceID findAudio(){AudioObjectPropertyAddress a{kAudioHardwarePropertyDevices,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};UInt32 z=0;check(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,&a,0,nullptr,&z),"devices size");std::vector<AudioDeviceID> ds(z/sizeof(AudioDeviceID));check(AudioObjectGetPropertyData(kAudioObjectSystemObject,&a,0,nullptr,&z,ds.data()),"devices");std::vector<virus::identity::AudioCandidate> candidates;for(auto d:ds)candidates.push_back({d,virus::identity::family(audioName(d))});return virus::identity::selectAudioDevice(candidates);}
static NSDictionary* inspectAudio(AudioDeviceID d){NSMutableDictionary* result=[NSMutableDictionary dictionary];result[@"device_id"]=@(d);result[@"name"]=@(audioName(d).c_str());
 for(auto pair:{std::pair<const char*,AudioObjectPropertyScope>{"input",kAudioDevicePropertyScopeInput},{"output",kAudioDevicePropertyScopeOutput}}){
  NSMutableDictionary* scope=[NSMutableDictionary dictionary];AudioObjectPropertyAddress a{kAudioDevicePropertyStreamConfiguration,pair.second,kAudioObjectPropertyElementMain};UInt32 z=0;check(AudioObjectGetPropertyDataSize(d,&a,0,nullptr,&z),"stream config size");std::vector<uint8_t> bytes(z);check(AudioObjectGetPropertyData(d,&a,0,nullptr,&z,bytes.data()),"stream config");auto* b=(AudioBufferList*)bytes.data();NSMutableArray* buffers=[NSMutableArray array];for(UInt32 i=0;i<b->mNumberBuffers;i++)[buffers addObject:@{ @"channels":@(b->mBuffers[i].mNumberChannels),@"bytes":@(b->mBuffers[i].mDataByteSize)}];scope[@"buffers"]=buffers;
  a.mSelector=kAudioDevicePropertyStreams;check(AudioObjectGetPropertyDataSize(d,&a,0,nullptr,&z),"streams size");std::vector<AudioStreamID> streams(z/sizeof(AudioStreamID));check(AudioObjectGetPropertyData(d,&a,0,nullptr,&z,streams.data()),"streams");NSMutableArray* formats=[NSMutableArray array];
  for(auto stream:streams){AudioObjectPropertyAddress f{kAudioStreamPropertyVirtualFormat,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};AudioStreamBasicDescription fmt{};z=sizeof(fmt);check(AudioObjectGetPropertyData(stream,&f,0,nullptr,&z,&fmt),"format");[formats addObject:@{ @"stream_id":@(stream),@"rate":@(fmt.mSampleRate),@"format_id":@(fmt.mFormatID),@"flags":@(fmt.mFormatFlags),@"bytes_per_frame":@(fmt.mBytesPerFrame),@"channels":@(fmt.mChannelsPerFrame),@"bits":@(fmt.mBitsPerChannel)}];}scope[@"formats"]=formats;result[@(pair.first)]=scope;
 }
 return result;
}
using virus::hardware::Midi;

int main(int argc,char** argv){@autoreleasepool{try{
 if(argc!=3&&!(argc==4&&std::string(argv[1])=="--restore-multi"))throw std::runtime_error("usage: hardware-bench MODE OUTPUT_DIRECTORY [MULTI_BACKUP]");
 if(std::string(argv[1])=="--plugin-notes-from-single")throw std::runtime_error("Quarantined: this mode transition stopped hardware MIDI replies; do not repeat until its control handshake is resolved");
 NSString* dir=@(argv[2]);[[NSFileManager defaultManager]createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
 NSMutableDictionary* result=[NSMutableDictionary dictionaryWithDictionary:inspectAudio(findAudio())];
 if(std::string(argv[1])=="--restore-multi"){
  std::ifstream file(argv[3],std::ios::binary);std::vector<uint8_t> saved{std::istreambuf_iterator<char>(file),{}};
  if(saved.size()!=267||saved[0]!=0xf0||saved[6]!=0x11||saved[7]!=0||saved[8]!=127||saved.back()!=0xf7)throw std::runtime_error("Not a multi edit-buffer snapshot");unsigned sum=0;for(int j=5;j<265;j++)sum+=saved[j];if((sum&127)!=saved[265])throw std::runtime_error("Multi snapshot checksum invalid");
  Midi midi;auto before=midi.multi();[[NSData dataWithBytes:before.data() length:before.size()]writeToFile:[dir stringByAppendingPathComponent:@"before-restore.syx"] atomically:YES];midi.send(saved);std::this_thread::sleep_for(std::chrono::milliseconds(500));auto after=midi.multi();[[NSData dataWithBytes:after.data() length:after.size()]writeToFile:[dir stringByAppendingPathComponent:@"after-restore.syx"] atomically:YES];result[@"multi_restored_exactly"]=@(after==saved);
 }else if(std::string(argv[1])=="--restore-single-routing"){
  Midi midi;for(int index:{58,59,60})midi.send({0xf0,0,0x20,0x33,1,0x10,0x73,0,(uint8_t)index,1,0xf7});for(int i=0;i<2;i++){midi.send({0xf0,0,0x20,0x33,1,0x10,0x73,0,122,0,0xf7});std::this_thread::sleep_for(std::chrono::milliseconds(350));}midi.send({0xf0,0,0x20,0x33,1,0x10,0x72,0,41,1,0xf7});std::this_thread::sleep_for(std::chrono::milliseconds(100));result[@"standalone_restore_commands_sent"]=@YES;
 }else if(std::string(argv[1])=="--query"||std::string(argv[1])=="--private-query"){
  Midi midi;if(std::string(argv[1])=="--private-query")midi.usePrivate();midi.request(0x30);std::this_thread::sleep_for(std::chrono::milliseconds(800));midi.request(0x30,0);std::this_thread::sleep_for(std::chrono::milliseconds(800));midi.request(0x31,0);std::this_thread::sleep_for(std::chrono::milliseconds(800));midi.send({0xf0,0,0x20,0x33,1,0x10,0x35,0xf7});std::this_thread::sleep_for(std::chrono::milliseconds(800));midi.savePrivate(dir);
  auto messages=midi.received();NSMutableArray* summaries=[NSMutableArray array];unsigned n=0;
  for(auto& m:messages){auto source=midi.origin(n);NSString* name=[NSString stringWithFormat:@"response-%03u.syx",n++];[[NSData dataWithBytes:m.data() length:m.size()]writeToFile:[dir stringByAppendingPathComponent:name] atomically:YES];[summaries addObject:@{ @"file":name,@"bytes":@(m.size()),@"command":@(m.size()>6?m[6]:0),@"source":@(source.c_str())}];}result[@"midi_responses"]=summaries;
 }else if(std::string(argv[1])=="--capture"||std::string(argv[1])=="--notes"||std::string(argv[1])=="--cc-check"||std::string(argv[1])=="--cc-check-sysex-single"||std::string(argv[1])=="--plugin-notes-from-single"||std::string(argv[1])=="--private-plugin-notes"||std::string(argv[1])=="--private-init-notes"){
  const bool initTest=std::string(argv[1])=="--private-init-notes";
  const bool privateTest=std::string(argv[1])=="--private-plugin-notes"||initTest;
  const bool routeTest=std::string(argv[1])=="--plugin-notes-from-single"||privateTest;
  const bool notes=std::string(argv[1])=="--notes"||routeTest;
  const bool sysexSingle=std::string(argv[1])=="--cc-check-sysex-single";
  const bool testCC=std::string(argv[1])=="--cc-check"||sysexSingle;
  NSDictionary* fmt=result[@"input"][@"formats"][0];
  if([fmt[@"rate"]doubleValue]!=48000||[fmt[@"channels"]intValue]!=6||[fmt[@"bits"]intValue]!=32||[fmt[@"flags"]intValue]!=9)throw std::runtime_error("Unsupported capture format; no device settings changed");
  Midi midi;if(privateTest)midi.usePrivate();std::array<std::vector<uint8_t>,2> backups;std::vector<uint8_t> multiBackup;
  if(testCC||routeTest)for(int i=0;i<2;i++){backups[i]=midi.single(i?0x40:0);if(![[NSData dataWithBytes:backups[i].data() length:backups[i].size()]writeToFile:[dir stringByAppendingPathComponent:[NSString stringWithFormat:@"before-part-%d.syx",i?64:0]] atomically:YES])throw std::runtime_error("Failed to save single backup; no mutation performed");}
  if(routeTest){multiBackup=midi.multi();if(![[NSData dataWithBytes:multiBackup.data() length:multiBackup.size()]writeToFile:[dir stringByAppendingPathComponent:@"before-multi.syx"] atomically:YES])throw std::runtime_error("Failed to save multi backup; no mutation performed");}
  auto restoreCC=[&](){if(testCC)for(int i=0;i<2;i++)midi.send({0xf0,0,0x20,0x33,1,0x10,0x70,(uint8_t)(i?0x40:0),40,backups[i][49],0xf7});};
  auto restoreRoute=[&](){if(routeTest){std::string failures;auto attempt=[&](auto action){try{action();}catch(const std::exception& e){failures+=std::string(e.what())+"; ";}};if(initTest)attempt([&]{midi.send(backups[0]);std::this_thread::sleep_for(std::chrono::milliseconds(250));});attempt([&]{midi.send({0xf0,0,0x20,0x33,1,0x10,0x72,0,41,multiBackup[185],0xf7});});attempt([&]{midi.send({0xf0,0,0x20,0x33,1,0x10,0x73,0,122,0,0xf7});});std::this_thread::sleep_for(std::chrono::milliseconds(350));attempt([&]{midi.send(multiBackup);});std::this_thread::sleep_for(std::chrono::milliseconds(100));if(!failures.empty())throw std::runtime_error("Restoration errors: "+failures);}};
  BenchHost host(notes,testCC);AudioDeviceIOProcID proc=nullptr;auto device=findAudio();
  check(AudioDeviceCreateIOProcID(device,BenchHost::callback,&host,&proc),"Create audio callback");
  NSMutableArray* sent=[NSMutableArray array];
  try{
   if(routeTest){midi.send({0xf0,0,0x20,0x33,1,0x10,0x73,0,122,3,0xf7});std::this_thread::sleep_for(std::chrono::milliseconds(350));midi.send({0xf0,0,0x20,0x33,1,0x10,0x72,0,41,10,0xf7});auto routed=midi.multi();[[NSData dataWithBytes:routed.data() length:routed.size()]writeToFile:[dir stringByAppendingPathComponent:@"routed-multi.syx"] atomically:YES];result[@"routing_readback"]=@(routed[185]);if(routed[185]!=10)throw std::runtime_error("USB routing readback failed");}
   if(initTest){std::ifstream f("analysis/hardware/fixtures/stock-wavetable-probe.syx",std::ios::binary);std::vector<uint8_t> patch{std::istreambuf_iterator<char>(f),{}};if(patch.size()!=524||patch[6]!=0x10||patch[7]!=0)throw std::runtime_error("Invalid known tone patch");patch[8]=0;patch[100]=45;unsigned sum=0;for(size_t i=5;i<265;i++)sum+=patch[i];patch[265]=sum&127;sum=0;for(size_t i=5;i<522;i++)sum+=patch[i];patch[522]=sum&127;midi.send(patch);std::this_thread::sleep_for(std::chrono::milliseconds(500));auto readback=midi.single(0);[[NSData dataWithBytes:readback.data() length:readback.size()]writeToFile:[dir stringByAppendingPathComponent:@"test-patch-readback.syx"] atomically:YES];result[@"test_patch_exact"]=@(readback==patch);if(readback!=patch)throw std::runtime_error("Tone patch readback differs");}
   check(AudioDeviceStart(device,proc),"Start audio");auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(11);
   while(host.frames.load(std::memory_order_acquire)<BenchHost::maxFrames&&std::chrono::steady_clock::now()<deadline){
    virus::bridge::MidiMessage m;while(host.bridge.serviceTakeMidi(m)){if(sysexSingle){if(m.bytes[0]!=0xb0||m.bytes[1]!=40)throw std::runtime_error("Single SysEx test only maps part1 cutoff");midi.send({0xf0,0,0x20,0x33,1,0x10,0x70,0x40,40,m.bytes[2],0xf7});}else midi.send({m.bytes.begin(),m.bytes.end()});[sent addObject:@{@"block_start":@(m.block_start),@"sample_offset":@(m.sample_offset),@"status":@(m.bytes[0]),@"data1":@(m.bytes[1]),@"data2":@(m.bytes[2]),@"single_sysex_adapter":@(sysexSingle),@"delivery":privateTest?@"Hidden USB interface route 2 after completed VST3 block":@"CoreMIDI immediate after completed VST3 block"}];}
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
   }
   check(AudioDeviceStop(device,proc),"Stop audio");check(AudioDeviceDestroyIOProcID(device,proc),"Destroy audio callback");proc=nullptr;
  }catch(...){if(proc){AudioDeviceStop(device,proc);AudioDeviceDestroyIOProcID(device,proc);}auto attempt=[](auto action){try{action();}catch(const std::exception& e){std::cerr<<"Cleanup: "<<e.what()<<"\n";}};if(notes)for(int key:{60,64,68})attempt([&]{midi.send({0x80,(uint8_t)key,0});});attempt(restoreCC);attempt(restoreRoute);attempt([&]{midi.savePrivate(dir);});throw;}
  try{
  if(notes)for(int key:{60,64,68})midi.send({0x80,(uint8_t)key,0});
  if(routeTest){restoreRoute();auto restored=midi.multi();[[NSData dataWithBytes:restored.data() length:restored.size()]writeToFile:[dir stringByAppendingPathComponent:@"restored-multi.syx"] atomically:YES];result[@"multi_restored_exactly"]=@(restored==multiBackup);for(int i=0;i<2;i++){auto s=midi.single(i?64:0);[[NSData dataWithBytes:s.data() length:s.size()]writeToFile:[dir stringByAppendingPathComponent:[NSString stringWithFormat:@"restored-part-%d.syx",i?64:0]] atomically:YES];if(s!=backups[i])throw std::runtime_error("Patch changed during mode test; backups preserved");}for(size_t i=0;i<restored.size();i++)if(i!=10&&i!=265&&restored[i]!=multiBackup[i])throw std::runtime_error("Multi parameter restoration mismatch");result[@"multi_restored_except_header_and_checksum"]=@YES;result[@"mode_restore"]=@"Single (original mode confirmed by user; SysEx mode 0 sent)";}
  if(testCC){NSMutableArray* checks=[NSMutableArray array];try{for(int i=0;i<2;i++){auto changed=midi.single(i?64:0);[[NSData dataWithBytes:changed.data() length:changed.size()]writeToFile:[dir stringByAppendingPathComponent:[NSString stringWithFormat:@"changed-part-%d.syx",i?64:0]] atomically:YES];[checks addObject:@{@"part":@(i?64:0),@"before_cutoff":@(backups[i][49]),@"after_cutoff":@(changed[49])}];}}catch(...){restoreCC();throw;}restoreCC();for(int i=0;i<2;i++){auto restored=midi.single(i?64:0);[[NSData dataWithBytes:restored.data() length:restored.size()]writeToFile:[dir stringByAppendingPathComponent:[NSString stringWithFormat:@"restored-part-%d.syx",i?64:0]] atomically:YES];if(restored!=backups[i])throw std::runtime_error("Restored patch mismatch; backups saved");}result[@"cc_readback"]=checks;result[@"cc_restored_exactly"]=@YES;}
  midi.savePrivate(dir);auto count=host.frames.load();writeWave(std::string(argv[2])+"/raw-usb.wav",host.raw,count);writeWave(std::string(argv[2])+"/vst3-returns.wav",host.rendered,count);
  NSMutableArray* peaks=[NSMutableArray array];double maxError=0;
  for(int c=0;c<6;c++){double peak=0,squares=0;for(uint64_t f=0;f<count;f++){auto v=host.rendered[f*6+c];peak=std::max(peak,std::abs(double(v)));squares+=v*v;maxError=std::max(maxError,std::abs(double(v)-host.raw[f*6+(c+2)%6]));}[peaks addObject:@{@"channel":@(c+1),@"peak":@(peak),@"rms":@(count?sqrt(squares/count):0)}];}
  const auto& stats=host.bridge.stats();result[@"stream_test"]=@{@"frames":@(count),@"callbacks":@(host.callbacks),@"callback_errors":@(host.errors),@"timestamp_gaps":@(host.timestampGaps),@"publish_failures":@(host.publishFailures),@"bridge_rendered":@(stats.rendered),@"bridge_underruns":@(stats.underrun),@"bridge_midi_dropped":@(stats.midi_dropped),@"bridge_midi_queued":@(stats.midi_queued),@"max_raw_vs_plugin_error":@(maxError),@"plugin_channel_to_raw_channel":@[@3,@4,@5,@6,@1,@2],@"channels":peaks,@"sent_midi":sent,@"plugin":@(VTR_PLUGIN_PATH)};
  }catch(...){auto attempt=[](auto action){try{action();}catch(const std::exception& e){std::cerr<<"Finalization cleanup: "<<e.what()<<"\n";}};if(notes)for(int key:{60,64,68})attempt([&]{midi.send({0x80,(uint8_t)key,0});});attempt(restoreCC);attempt(restoreRoute);attempt([&]{midi.savePrivate(dir);});throw;}
 }else if(std::string(argv[1])!="--inspect")throw std::runtime_error("Unsupported mode");
 NSData* json=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:nil];[json writeToFile:[dir stringByAppendingPathComponent:@"result.json"] atomically:YES];std::cout<<std::string((const char*)json.bytes,json.length)<<std::endl;return 0;
 }catch(const std::exception& e){std::cerr<<"ERROR "<<e.what()<<std::endl;return 1;}}}
