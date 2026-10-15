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
 if(argc==3){std::signal(SIGTERM,[](int){stopHold=true;});assert(AudioUnitInitialize(unit)==noErr);std::cout<<"AU_HOLD_READY"<<std::endl;auto end=std::chrono::steady_clock::now()+std::chrono::seconds(240);double frame=0,renderedPeak=0;
  while(!stopHold&&std::chrono::steady_clock::now()<end){float l[256]{},r[256]{};struct {UInt32 n;AudioBuffer b[2];} a{2,{{1,sizeof l,l},{1,sizeof r,r}}};AudioTimeStamp t{};t.mFlags=kAudioTimeStampSampleTimeValid;t.mSampleTime=frame;AudioUnitRenderActionFlags flags=0;assert(AudioUnitRender(unit,&flags,&t,0,256,(AudioBufferList*)&a)==noErr);for(float sample:l)renderedPeak=std::max(renderedPeak,std::abs(double(sample)));frame+=256;std::this_thread::sleep_for(std::chrono::microseconds(2667));}
  std::cout<<"AU_RENDER_PEAK "<<renderedPeak<<std::endl;AudioUnitUninitialize(unit);
 }
 assert(AudioComponentInstanceDispose(unit)==noErr);CFRelease(bundle);std::cout<<"AU: registration, three stereo buses, MIDI/render at 44.1/48/96k, state and Cocoa UI property passed\n";
}return 0;}
