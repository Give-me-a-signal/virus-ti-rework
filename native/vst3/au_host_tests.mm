#import <Cocoa/Cocoa.h>
#import <AudioUnit/AudioUnit.h>
#import <AudioUnit/AUCocoaUIView.h>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <cmath>
static std::atomic<bool> stopHold{false};
#import <AudioToolbox/AudioToolbox.h>
#include <cassert>
#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
static void stateChanged(void* context, AudioUnit, AudioUnitPropertyID, AudioUnitScope, AudioUnitElement) {
 ++*static_cast<unsigned*>(context);
}
static NSDictionary* control(NSString* endpoint, NSDictionary* command) {
 int fd=socket(AF_UNIX,SOCK_STREAM,0);assert(fd>=0);timeval timeout{5,0};assert(setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout)==0);sockaddr_un address{};address.sun_family=AF_UNIX;
 assert(strlen(endpoint.fileSystemRepresentation)<sizeof address.sun_path);strcpy(address.sun_path,endpoint.fileSystemRepresentation);
 assert(connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof address)==0);
 NSMutableData* wire=[[NSJSONSerialization dataWithJSONObject:command options:0 error:nil] mutableCopy];[wire appendBytes:"\n" length:1];
 size_t sent=0;while(sent<wire.length){auto n=write(fd,static_cast<const char*>(wire.bytes)+sent,wire.length-sent);assert(n>0);sent+=n;}
 NSMutableData* reply=[NSMutableData data];char buffer[4096];
 while(true){auto n=read(fd,buffer,sizeof buffer);assert(n>0);[reply appendBytes:buffer length:n];assert(reply.length<65536);if(buffer[n-1]=='\n')break;}
 close(fd);[wire release];NSDictionary* result=[NSJSONSerialization JSONObjectWithData:reply options:0 error:nil];if(![result[@"ok"] boolValue])NSLog(@"AU state test control error: %@",result);assert([result[@"ok"] boolValue]);return result;
}
static void nonParameterStateTest(AudioUnit unit) {
 // Model a host which caches ClassInfo until a dirty notification. Stage a
 // wholly synthetic RAM patch through the API; never Connect or perform USB I/O.
 NSString* directory=[NSString stringWithFormat:@"/tmp/virus-ti-control-%u",getuid()];NSString* endpoint=nil;
 NSString* prefix=[NSString stringWithFormat:@"%d-",getpid()];
 for(NSString* name in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:directory error:nil])
  if([name hasPrefix:prefix]&&[name hasSuffix:@".sock"]){assert(!endpoint);endpoint=[directory stringByAppendingPathComponent:name];}
 assert(endpoint);unsigned changes=0;assert(AudioUnitAddPropertyListener(unit,kAudioUnitProperty_ClassInfo,stateChanged,&changes)==noErr);
 CFPropertyListRef cached=nullptr;UInt32 size=sizeof cached;assert(AudioUnitGetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&cached,&size)==noErr);
 NSUInteger originalSize=[((NSDictionary*)cached)[@"Processor State"] length];
 std::vector<unsigned char> patch(524,0);patch[0]=0xf0;patch[2]=0x20;patch[3]=0x33;patch[4]=1;patch[5]=16;patch[6]=16;patch[9]=12;patch[49]=83;patch.back()=0xf7;
 unsigned sum=0;for(unsigned i=5;i<265;++i)sum+=patch[i];patch[265]=sum&127;sum=0;for(unsigned i=5;i<522;++i)sum+=patch[i];patch[522]=sum&127;
 NSData* initial=((NSDictionary*)cached)[@"Processor State"];assert(initial.length>12);
 const auto* bytes=static_cast<const unsigned char*>(initial.bytes);auto u32=[&](size_t at){return uint32_t(bytes[at])|(uint32_t(bytes[at+1])<<8)|(uint32_t(bytes[at+2])<<16)|(uint32_t(bytes[at+3])<<24);};
 size_t firstBlob=12+12*u32(8)+8;assert(firstBlob+8<=initial.length&&u32(firstBlob)==0);
 NSMutableData* synthetic=[initial mutableCopy];NSMutableData* blob=[NSMutableData data];auto append=[&](uint32_t value){unsigned char b[4];for(unsigned i=0;i<4;++i)b[i]=value>>(8*i);[blob appendBytes:b length:4];};
 append(patch.size());[blob appendBytes:patch.data() length:patch.size()];uint32_t crc=~0u;for(auto b:patch){crc^=b;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int(crc&1)));}append(~crc);
 [synthetic replaceBytesInRange:NSMakeRange(firstBlob,8) withBytes:blob.bytes length:blob.length];
 NSString* fixture=[NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"vtr-au-state-%d.vtrstate",getpid()]];
 assert([synthetic writeToFile:fixture atomically:YES]);[synthetic release];
 auto result=control(endpoint,@{@"command":@"state-load",@"path":fixture});assert([result[@"last_command_succeeded"] boolValue]);
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
 while(!changes&&std::chrono::steady_clock::now()<deadline)[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.05]];
 assert(changes>0);CFRelease(cached);cached=nullptr;size=sizeof cached;
 assert(AudioUnitGetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&cached,&size)==noErr);
 NSData* saved=((NSDictionary*)cached)[@"Processor State"];assert(saved.length==originalSize+524);
 // Invalid payloads must propagate failure through the AU boundary and leave
 // the previous complete state intact (the upstream wrapper ignored this error).
 NSMutableDictionary* invalid=[((NSDictionary*)cached) mutableCopy];NSMutableData* corrupt=[saved mutableCopy];
 static_cast<unsigned char*>(corrupt.mutableBytes)[firstBlob+4+524]^=1;invalid[@"Processor State"]=corrupt;
 CFPropertyListRef bad=(CFPropertyListRef)invalid;assert(AudioUnitSetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&bad,sizeof bad)==kAudioUnitErr_InvalidPropertyValue);
 [corrupt release];[invalid release];
 // Repeated AU ClassInfo round trips retain the staged sound without connecting.
 for(unsigned i=0;i<10;++i){assert(AudioUnitSetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&cached,sizeof cached)==noErr);CFPropertyListRef next=nullptr;size=sizeof next;assert(AudioUnitGetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&next,&size)==noErr);assert([((NSDictionary*)next)[@"Processor State"] isEqual:saved]);CFRelease(next);}
 auto status=control(endpoint,@{@"command":@"status"});assert(![status[@"connected"] boolValue]&&![status[@"device_lease_held"] boolValue]);assert([status[@"lifetime_counters"][@"capture_callbacks"] unsignedLongLongValue]==0);
 assert([status[@"lifetime_counters"][@"midi_sent"] unsignedLongLongValue]==0);
 CFRelease(cached);AudioUnitRemovePropertyListenerWithUserData(unit,kAudioUnitProperty_ClassInfo,stateChanged,&changes);[[NSFileManager defaultManager] removeItemAtPath:fixture error:nil];
}
int main(int argc,char** argv){@autoreleasepool {
 assert(argc==2||argc==3);[NSApplication sharedApplication];NSURL* url=[NSURL fileURLWithPath:@(argv[1])];CFBundleRef bundle=CFBundleCreate(nullptr,(__bridge CFURLRef)url);assert(bundle&&CFBundleLoadExecutable(bundle));
 auto factory=(AudioComponentFactoryFunction)CFBundleGetFunctionPointerForName(bundle,CFSTR("AUWrapperFactory"));assert(factory);
 AudioComponentDescription desc{'aumu','VtRw','GmAS',0,0};AudioComponent comp=AudioComponentRegister(&desc,CFSTR("Virus TI Rework test"),8193,factory);assert(comp);
 AudioUnit unit=nullptr;assert(AudioComponentInstanceNew(comp,&unit)==noErr);UInt32 buses=0,size=sizeof buses;assert(AudioUnitGetProperty(unit,kAudioUnitProperty_ElementCount,kAudioUnitScope_Output,0,&buses,&size)==noErr);assert(buses==3);
 AudioStreamBasicDescription format{};size=sizeof format;assert(AudioUnitGetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,0,&format,&size)==noErr);assert(format.mChannelsPerFrame==2);
 for(Float64 rate:{44100.,48000.,96000.}){format.mSampleRate=rate;for(unsigned b=0;b<buses;++b)assert(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,b,&format,sizeof format)==noErr);assert(AudioUnitInitialize(unit)==noErr);
  assert(MusicDeviceMIDIEvent(unit,0x90,60,100,0)==noErr);assert(MusicDeviceMIDIEvent(unit,0x80,60,0,32)==noErr);
  float left[128]{},right[128]{};struct {UInt32 count;AudioBuffer buffers[2];} list{2,{{1,sizeof left,left},{1,sizeof right,right}}};AudioTimeStamp time{};time.mFlags=kAudioTimeStampSampleTimeValid;AudioUnitRenderActionFlags flags=0;
  for(unsigned b=0;b<buses;++b)assert(AudioUnitRender(unit,&flags,&time,b,128,(AudioBufferList*)&list)==noErr);
  assert(AudioUnitUninitialize(unit)==noErr);
 }
 CFPropertyListRef state=nullptr;size=sizeof state;assert(AudioUnitGetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&state,&size)==noErr&&state);assert(AudioUnitSetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&state,sizeof state)==noErr);CFRelease(state);
 // Exercise a real Cocoa editor, not just its property declaration.
size=0;Boolean writable=false;assert(AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_CocoaUI,kAudioUnitScope_Global,0,&size,&writable)==noErr&&size>0);
 std::vector<uint8_t> uiBytes(size);assert(AudioUnitGetProperty(unit,kAudioUnitProperty_CocoaUI,kAudioUnitScope_Global,0,uiBytes.data(),&size)==noErr);
 auto* ui=reinterpret_cast<AudioUnitCocoaViewInfo*>(uiBytes.data());NSBundle* viewBundle=[NSBundle bundleWithURL:(__bridge NSURL*)ui->mCocoaAUViewBundleLocation];assert([viewBundle load]);Class viewClass=[viewBundle classNamed:(__bridge NSString*)ui->mCocoaAUViewClass[0]];assert(viewClass);id<AUCocoaUIBase> factoryView=[viewClass new];NSView* view=[factoryView uiViewForAudioUnit:unit withSize:NSMakeSize(907,513)];assert(view);[view setFrameSize:NSMakeSize(1360.5,769.5)];
 CFRelease(ui->mCocoaAUViewBundleLocation);CFRelease(ui->mCocoaAUViewClass[0]);
 if(argc==2)nonParameterStateTest(unit);
 if(argc==3){std::signal(SIGTERM,[](int){stopHold=true;});assert(AudioUnitInitialize(unit)==noErr);std::cout<<"AU_HOLD_READY"<<std::endl;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(240);double frame=0,renderedPeak=0;
  while(!stopHold&&std::chrono::steady_clock::now()<end){float l[256]{},r[256]{};struct {UInt32 n;AudioBuffer b[2];} a{2,{{1,sizeof l,l},{1,sizeof r,r}}};AudioTimeStamp t{};t.mFlags=kAudioTimeStampSampleTimeValid;t.mSampleTime=frame;AudioUnitRenderActionFlags flags=0;assert(AudioUnitRender(unit,&flags,&t,0,256,(AudioBufferList*)&a)==noErr);for(float sample:l)renderedPeak=std::max(renderedPeak,std::abs(double(sample)));frame+=256;std::this_thread::sleep_for(std::chrono::microseconds(2667));}
  std::cout<<"AU_RENDER_PEAK "<<renderedPeak<<std::endl;AudioUnitUninitialize(unit);
 }
 assert(AudioComponentInstanceDispose(unit)==noErr);CFRelease(bundle);std::cout<<"AU: registration, three stereo buses, MIDI/render at 44.1/48/96k, state and Cocoa UI property passed\n";
}return 0;}
