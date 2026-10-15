#pragma once
#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <IOUSBHost/IOUSBHost.h>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <array>
#include "usb_midi1.hpp"
#include "virus/usb_descriptors.hpp"
#include "../vst3/timed_midi.h"

namespace virus::private_midi {
inline int number(io_service_t service,CFStringRef key){id v=CFBridgingRelease(IORegistryEntryCreateCFProperty(service,key,kCFAllocatorDefault,0));if(![v isKindOfClass:NSNumber.class])throw std::runtime_error("Missing USB identity property");return [v intValue];}
inline void require(bool ok,NSError* e,const char* operation){if(!ok)throw std::runtime_error(std::string(operation)+": "+(e?e.description.UTF8String:"missing object"));}
class PrivateUSB {
 IOUSBHostInterface* interface_=nil;
 IOUSBHostPipe* in_=nil;
 IOUSBHostPipe* out_=nil;
 std::atomic<bool> stopping_{false};
 std::thread reader_;
 std::mutex logLock_;
 std::mutex writeLock_;
 std::vector<std::vector<uint8_t>> rawReads_,rawWrites_;
 std::array<std::vector<uint8_t>,16> assembling_;
 std::string readError_;
 // A control-thread operation, never called on the audio callback. Interrupt
 // requests require API timeout=0, so enforce a deadline by aborting this pipe
 // and waiting for the real completion before releasing its buffer.
 std::vector<uint8_t> transfer(IOUSBHostPipe* pipe,const std::vector<uint8_t>& bytes,unsigned timeoutMs,bool reading){
  NSMutableData* data=[NSMutableData dataWithBytes:bytes.data() length:bytes.size()];
  dispatch_semaphore_t done=dispatch_semaphore_create(0);__block IOReturn status=kIOReturnSuccess;__block NSUInteger count=0;
  NSError* error=nil;
  BOOL accepted=[pipe enqueueIORequestWithData:data completionTimeout:0 error:&error completionHandler:^(IOReturn s,NSUInteger n){status=s;count=n;dispatch_semaphore_signal(done);}];
  require(accepted,error,"Submit private MIDI interrupt I/O");
  const bool timedOut=dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,int64_t(timeoutMs)*1000000))!=0;
  if(timedOut){BOOL aborted=[pipe abortWithOption:IOUSBHostAbortOptionSynchronous error:&error];if(!aborted)fprintf(stderr,"Private MIDI abort failed; retaining request and waiting for real completion: %s\n",error.description.UTF8String);dispatch_semaphore_wait(done,DISPATCH_TIME_FOREVER);}
  if(timedOut&&reading&&status==kIOReturnAborted){if(count>data.length||count%4)throw std::runtime_error("Aborted private read returned partial word");auto* p=static_cast<const uint8_t*>(data.bytes);return {p,p+count};}
  if(status!=kIOReturnSuccess)throw std::runtime_error("Private MIDI completion status "+std::to_string(status));
  if(count>data.length||(!reading&&count!=bytes.size()))throw std::runtime_error("Private MIDI transfer length mismatch");
  auto* p=static_cast<const uint8_t*>(data.bytes);return {p,p+count};
 }
public:
 uint64_t registryID=0;
 unsigned interfaceNumber=0,inAddress=0,outAddress=0,inType=0,outType=0;
 std::vector<uint8_t> configuration;
 PrivateUSB(){
  io_iterator_t iterator=0;auto match=IOServiceMatching("IOUSBHostInterface");
  if(IOServiceGetMatchingServices(kIOMainPortDefault,match,&iterator)!=KERN_SUCCESS)throw std::runtime_error("USB interface enumeration failed");
  struct Candidate{io_service_t service;int number;int location;};std::vector<Candidate> candidates;
  for(io_service_t s;(s=IOIteratorNext(iterator));){try{if(number(s,CFSTR("idVendor"))==0x133e&&number(s,CFSTR("idProduct"))==0x0815&&number(s,CFSTR("bInterfaceClass"))==255&&number(s,CFSTR("bInterfaceSubClass"))==0&&number(s,CFSTR("bInterfaceProtocol"))==0)candidates.push_back({s,number(s,CFSTR("bInterfaceNumber")),number(s,CFSTR("locationID"))});else IOObjectRelease(s);}catch(...){IOObjectRelease(s);}}
  IOObjectRelease(iterator);
  std::sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a.number<b.number;});
  if(candidates.size()!=2||candidates[0].location!=candidates[1].location){for(auto c:candidates)IOObjectRelease(c.service);throw std::runtime_error("Expected exactly two vendor interfaces on one TI Desktop");}
  // Legacy CreateHiddenPortInterface explicitly skips the first matching
  // vendor interface and uses the second. Do not claim the public MIDI port.
  auto selected=candidates[1];interfaceNumber=selected.number;IORegistryEntryGetRegistryEntryID(selected.service,&registryID);
  NSError* error=nil;
  interface_=[[IOUSBHostInterface alloc]initWithIOService:selected.service options:IOUSBHostObjectInitOptionsNone queue:nil error:&error interestHandler:nil];
  for(auto c:candidates)IOObjectRelease(c.service);
  require(interface_!=nil,error,"Open hidden interface without seize");
  try{
   const auto* descriptor=interface_.interfaceDescriptor;
   if(!descriptor||descriptor->bInterfaceNumber!=interfaceNumber||descriptor->bAlternateSetting!=0||descriptor->bInterfaceClass!=255)throw std::runtime_error("Unexpected hidden interface descriptor");
   // Use the SDK's actual-length descriptor API instead of trusting wTotalLength.
   NSUInteger length=65535;auto* raw=[interface_ descriptorWithType:(tIOUSBDescriptorType)2 length:&length index:0 languageID:0 error:&error];
   require(raw&&length>=9&&length<=65535,error,"Read configuration descriptor");
   auto bytes=reinterpret_cast<const uint8_t*>(raw);configuration.assign(bytes,bytes+length);
   auto config=usb::parse_configuration(configuration);unsigned found=0;
   for(const auto& alternate:config.alternates)if(alternate.interface_number==interfaceNumber&&alternate.alternate_setting==0){
    if(alternate.interface_class!=255||alternate.interface_subclass||alternate.interface_protocol||alternate.endpoints.size()!=2)throw std::runtime_error("Unexpected private interface topology");
    for(const auto& endpoint:alternate.endpoints){if(endpoint.transfer_type()!=2&&endpoint.transfer_type()!=3)throw std::runtime_error("Private endpoint is neither bulk nor interrupt");if(endpoint.address&128){if(inAddress)throw std::runtime_error("Duplicate IN");inAddress=endpoint.address;inType=endpoint.transfer_type();}else{if(outAddress)throw std::runtime_error("Duplicate OUT");outAddress=endpoint.address;outType=endpoint.transfer_type();}}++found;
   }
   if(found!=1||!inAddress||!outAddress)throw std::runtime_error("Missing private bulk pair");
   in_=[interface_ copyPipeWithAddress:inAddress error:&error];require(in_!=nil,error,"Open private IN pipe");
   out_=[interface_ copyPipeWithAddress:outAddress error:&error];require(out_!=nil,error,"Open private OUT pipe");
  }catch(...){[interface_ destroy];interface_=nil;throw;}
 }
 ~PrivateUSB(){stopReader();in_=nil;out_=nil;[interface_ destroy];interface_=nil;}
 void startReader(std::function<void(const std::vector<uint8_t>&)> message){
  if(reader_.joinable())throw std::runtime_error("Private reader already started");stopping_=false;
  reader_=std::thread([this,message](){@autoreleasepool{try{while(!stopping_.load()){
   std::vector<uint8_t> bytes;@autoreleasepool{bytes=transfer(in_,std::vector<uint8_t>(32),100,true);}if(bytes.empty())continue;
   {std::lock_guard<std::mutex> g(logLock_);if(rawReads_.size()<100000)rawReads_.push_back(bytes);}
   if(bytes.size()%4)throw std::runtime_error("Partial private MIDI word");
   for(size_t i=0;i<bytes.size();i+=4){midi::USBMidiEvent event{};if(midi::decode_usb_midi1_packet(bytes.data()+i,4,event)!=midi::USBMidiResult::ok)continue;
    auto& assembly=assembling_[event.cable];for(int n=0;n<event.size;n++){auto b=event.bytes[n];if(b>=0xf8)continue;if(b==0xf0)assembly.clear();if(b==0xf0||!assembly.empty()){if(assembly.size()>=65536)throw std::runtime_error("Private SysEx too large");assembly.push_back(b);if(b==0xf7){message(assembly);assembly.clear();}}}
   }
  }}catch(const std::exception& e){std::lock_guard<std::mutex> g(logLock_);readError_=e.what();}}});
 }
 void stopReader(){stopping_=true;if(reader_.joinable())reader_.join();}
 std::string readerError(){std::lock_guard<std::mutex> g(logLock_);return readError_;}
 void sendMessage(const std::vector<uint8_t>& bytes){
  std::lock_guard<std::mutex> writer(writeLock_);
  // Caller owns the semantic no-firmware allowlist; this method only frames
  // already-approved messages on the observed immediate private route 2.
  std::vector<midi::USBMidiPacket> packets(bytes.size()/3+1);size_t n=0;
  if(midi::encode_usb_midi1_message(2,bytes.data(),bytes.size(),packets.data(),packets.size(),n)!=midi::USBMidiResult::ok)throw std::runtime_error("Invalid private message");
  auto* p=reinterpret_cast<const uint8_t*>(packets.data());
  for(size_t pos=0;pos<n*4;pos+=32){std::vector<uint8_t> fragment(p+pos,p+std::min(pos+32,n*4));transfer(out_,fragment,1000,false);std::lock_guard<std::mutex> g(logLock_);rawWrites_.push_back(fragment);}
 }
 void sendTimed(const std::vector<uint8_t>& bytes,uint16_t stamp,bool clock=false){
  const auto packets=VirusRework::timedPackets(bytes,stamp,clock);
  std::lock_guard<std::mutex> writer(writeLock_);
  for(size_t pos=0;pos<packets.size();pos+=32){
   std::vector<uint8_t> fragment(packets.begin()+pos,packets.begin()+std::min(pos+32,packets.size()));
   transfer(out_,fragment,1000,false);std::lock_guard<std::mutex> g(logLock_);rawWrites_.push_back(fragment);
  }
 }
 void saveTraffic(NSString* directory){stopReader();std::lock_guard<std::mutex> g(logLock_);NSMutableData* reads=[NSMutableData data];NSMutableData* writes=[NSMutableData data];for(auto& p:rawReads_)[reads appendBytes:p.data() length:p.size()];for(auto& p:rawWrites_)[writes appendBytes:p.data() length:p.size()];[reads writeToFile:[directory stringByAppendingPathComponent:@"private-in.bin"] atomically:YES];[writes writeToFile:[directory stringByAppendingPathComponent:@"private-out.bin"] atomically:YES];}
 PrivateUSB(const PrivateUSB&)=delete;PrivateUSB& operator=(const PrivateUSB&)=delete;
};
}
