#include "restoration.h"
#include "../private-midi/hardware_midi.hpp"
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

int main(int argc,char** argv){
 // Exit the short-lived launcher before creating Objective-C/USB objects.
 // launchd reaps the orphan helper, even when the host only unloads the VST3.
 auto child=fork();if(child<0)return 1;if(child>0)_exit(0);
 @autoreleasepool{
 NSString* report=nil;
 auto status=[&](NSString* state,NSString* error=@""){
  NSDictionary* data=@{@"state":state,@"error":error,@"pid":@(getpid()),@"time":@([[NSDate date]timeIntervalSince1970]),@"firmware_operations_supported":@NO,@"stored_bank_writes_supported":@NO};
  NSData* bytes=[NSJSONSerialization dataWithJSONObject:data options:NSJSONWritingPrettyPrinted error:nil];
  if(!report||![bytes writeToFile:report atomically:YES])throw std::runtime_error("Cannot write restoration status");
 };
 try{
  if(argc!=3)throw std::runtime_error("Expected snapshot directory and USB registry identity");
  std::string directory=argv[1];NSString* root=@(directory.c_str());report=[root stringByAppendingPathComponent:@"restoration-status.json"];
  size_t parsed=0;auto expected=std::stoull(argv[2],&parsed);if(!expected||parsed!=std::string(argv[2]).size())throw std::runtime_error("Invalid USB identity");
  auto original=VirusRework::RestoreSnapshot::read(directory); // immutable private copy before host edits
  VirusRework::requireVerifiedProfile(original.model); // Never open Snow hardware from an unverified recovery file.
  status(@"armed");char armed='A';if(write(STDOUT_FILENO,&armed,1)!=1)throw std::runtime_error("Parent readiness pipe closed");
  char command=0;ssize_t count;do{count=read(STDIN_FILENO,&command,1);}while(count<0&&errno==EINTR);close(STDIN_FILENO);
  if(count==1&&command=='D'){status(@"completed-by-plugin");char done='D';write(STDOUT_FILENO,&done,1);return 0;}
  if(count<0)throw std::runtime_error("Lifeline read failed");
  status(@"waiting-for-device-release");
  // fd 3 shares the parent's flock lease and remains locked across host exit.
  if(fcntl(3,F_GETFD)<0||flock(3,LOCK_EX|LOCK_NB))throw std::runtime_error("Inherited device lease unavailable");
  std::unique_ptr<virus::hardware::Midi> device;
  std::string lastError;
  // USB/CoreMIDI teardown can lag a killed host by a short interval. Probe
  // without changing RAM before beginning the restoration transaction.
  for(unsigned attempt=0;attempt<10&&!device;++attempt){
   try{auto candidate=std::make_unique<virus::hardware::Midi>();candidate->usePrivate();
    if(candidate->registryID()!=expected)throw std::runtime_error("USB identity changed; refusing to restore another device");
    candidate->single(64);device=std::move(candidate);
   }catch(const std::exception& e){lastError=e.what();if(lastError.find("identity changed")!=std::string::npos)throw;std::this_thread::sleep_for(std::chrono::milliseconds(200));}
  }
  if(!device)throw std::runtime_error("Device did not become available: "+lastError);
  auto& midi=*device;
  auto output=[root stringByAppendingPathComponent:@"restoration-helper"];
  if(![[NSFileManager defaultManager]createDirectoryAtPath:output withIntermediateDirectories:NO attributes:nil error:nil])throw std::runtime_error("Cannot create restoration evidence directory");
  auto save=[&](std::string name,const std::vector<uint8_t>& bytes){if(![[NSData dataWithBytes:bytes.data() length:bytes.size()]writeToFile:[output stringByAppendingPathComponent:@(name.c_str())] atomically:YES])throw std::runtime_error("Cannot save restoration readback");};
  auto pause=[](unsigned ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));};
  status(@"restoring");
  try{VirusRework::restoreSnapshot(midi,original,save,pause);}catch(...){midi.savePrivate(output);throw;}
  midi.savePrivate(output);status(@"restored");std::cerr<<"All "<<VirusRework::partCount(original.model)<<" parts, Single buffer and multi verified; Single mode sent; USB normalized\n";
  // Device objects and lock are released by process exit after the evidence is durable.
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";try{status(@"failed",@(e.what()));}catch(...){}return 1;}
}}
