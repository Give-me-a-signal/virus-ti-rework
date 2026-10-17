#import <Cocoa/Cocoa.h>
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "base/source/fobject.h"
#include <iostream>
#include <memory>
#include <cmath>
using namespace Steinberg;
using namespace Steinberg::Vst;

// This host never initializes an audio device, calls setActive(true), starts
// processing, opens USB/MIDI, or enumerates third-party plug-ins.
class PreviewHost final:public FObject,public IComponentHandler,public IPlugFrame {
 VST3::Hosting::Module::Ptr module_;
 IPtr<HostApplication> application_;
 IPtr<PlugProvider> provider_;
 IPtr<IComponent> component_;
 IPtr<IEditController> controller_;
 IPtr<IAudioProcessor> processor_;
 IPtr<IPlugView> view_;
 __weak NSView* parent_=nil;
public:
 uint64_t edits=0;ParamID lastID=0;ParamValue lastValue=0;
 ~PreviewHost()override{close();}
 bool load(const std::string& path,std::string& error){
  close();application_=owned(new HostApplication);PluginContextFactory::instance().setPluginContext(application_);
  module_=VST3::Hosting::Module::create(path,error);if(!module_)return false;
  auto factory=module_->getFactory();factory.setHostContext(application_);
  // Use the permanent processor identity; display names changed for the beta.
  const VST3::UID processorID(0x5695B49E,0x529D4E17,0xA2855601,0xA10B277E);
  for(const auto& info:factory.classInfos())if(info.category()==kVstAudioEffectClass&&info.ID()==processorID){
   provider_=owned(new PlugProvider(factory,info,true));if(!provider_->initialize()){error="VST3 component/controller initialization failed";return false;}break;
  }
  if(!provider_){error="The expected rebuilt Virus TI Rework processor was not found";return false;}
  component_=provider_->getComponentPtr();controller_=provider_->getControllerPtr();
  if(!component_||!controller_){error="Rebuilt component or controller is missing";return false;}
  FUnknownPtr<IAudioProcessor> processor(component_);processor_=processor;
  if(!processor_){error="Audio processor interface is missing";return false;}
  ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;setup.maxSamplesPerBlock=128;setup.sampleRate=48000;
  if(processor_->setupProcessing(setup)!=kResultOk){error="Parameter-flush setup failed";return false;}
  MemoryStream state;if(component_->getState(&state)==kResultOk){state.seek(0,IBStream::kIBSeekSet,nullptr);controller_->setComponentState(&state);}
  controller_->setComponentHandler(this);view_=owned(controller_->createView(ViewType::kEditor));
  if(!view_||view_->isPlatformTypeSupported(kPlatformTypeNSView)!=kResultTrue){error="Native Cocoa editor is unavailable";return false;}
  return true;
 }
 bool attach(NSView* parent,std::string& error){
  if(!view_||!parent){error="No editor/parent to attach";return false;}
  parent_=parent;view_->setFrame(this);
  if(view_->attached((__bridge void*)parent,kPlatformTypeNSView)!=kResultTrue){view_->setFrame(nullptr);parent_=nil;error="Native IPlugView attachment failed (check bundle resources)";return false;}
  return true;
 }
 ViewRect size()const{ViewRect r{0,0,907,513};if(view_)view_->getSize(&r);return r;}
 void close(){
  if(view_){view_->removed();view_->setFrame(nullptr);view_=nullptr;}parent_=nil;
  if(controller_)controller_->setComponentHandler(nullptr);
  processor_=nullptr;controller_=nullptr;component_=nullptr;provider_=nullptr;
  if(module_)module_->getFactory().setHostContext(nullptr);
  module_.reset();PluginContextFactory::instance().setPluginContext(nullptr);application_=nullptr;
 }
 bool checkParameterRoundtrip(){
  if(controller_->getParameterCount()<1)return false;ParameterInfo p{};if(controller_->getParameterInfo(0,p)!=kResultOk)return false;
  controller_->setParamNormalized(p.id,.25);if(beginEdit(p.id)!=kResultOk||performEdit(p.id,.25)!=kResultOk||endEdit(p.id)!=kResultOk)return false;
  MemoryStream state;if(component_->getState(&state)!=kResultOk)return false;controller_->setParamNormalized(p.id,.75);state.seek(0,IBStream::kIBSeekSet,nullptr);
  return controller_->setComponentState(&state)==kResultOk&&controller_->getParamNormalized(p.id)==.25;
 }
 tresult PLUGIN_API beginEdit(ParamID id)override{return kResultOk;}
 tresult PLUGIN_API performEdit(ParamID id,ParamValue value)override{
  if(!processor_||!std::isfinite(value)||value<0||value>1)return kInvalidArgument;
  // A zero-sample flush propagates the UI edit into the actual inactive processor.
  // No audio callback/device is created, and the disconnected bridge remains off.
  ParameterChanges changes;int32 queueIndex=0,pointIndex=0;auto* q=changes.addParameterData(id,queueIndex);if(!q)return kResultFalse;q->addPoint(0,value,pointIndex);
  ProcessData data{};data.symbolicSampleSize=kSample32;data.numSamples=0;data.inputParameterChanges=&changes;
  auto result=processor_->process(data);if(result==kResultOk){++edits;lastID=id;lastValue=value;}return result;
 }
 tresult PLUGIN_API endEdit(ParamID id)override{return kResultOk;}
 tresult PLUGIN_API restartComponent(int32 flags)override{return kResultOk;}
 void windowSize(NSSize size){if(view_){ViewRect r{0,0,int32(size.width),int32(size.height)};view_->onSize(&r);}}
 tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* r)override{
  if(!view_||view!=view_.get()||!r)return kResultFalse;
  view_->checkSizeConstraint(r);NSSize size=NSMakeSize(r->getWidth(),r->getHeight());
  if(parent_.window)[parent_.window setContentSize:size];else[parent_ setFrameSize:size];return view_->onSize(r);
 }
 OBJ_METHODS(PreviewHost,FObject)
 DEFINE_INTERFACES
 DEF_INTERFACE(IComponentHandler)
 DEF_INTERFACE(IPlugFrame)
 END_DEFINE_INTERFACES(FObject)
 REFCOUNT_METHODS(FObject)
};

@interface PreviewApp : NSObject<NSApplicationDelegate,NSWindowDelegate> {
@public
 std::unique_ptr<PreviewHost> host;
 NSWindow* window;
 NSString* pluginPath;
}
@end
@implementation PreviewApp
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
 host=std::make_unique<PreviewHost>();std::string error;
 if(!host->load(pluginPath.UTF8String,error)){[self fail:@(error.c_str())];return;}
 auto r=host->size();window=[[NSWindow alloc]initWithContentRect:NSMakeRect(0,0,r.getWidth(),r.getHeight()) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
 window.title=@"Virus TI Rework — Native VST3 Interface (Offline)";window.delegate=self;window.releasedWhenClosed=NO;window.contentMinSize=NSMakeSize(907,513);window.contentMaxSize=NSMakeSize(1814,1026);window.contentAspectRatio=NSMakeSize(907,513);
 if(!host->attach(window.contentView,error)){[self fail:@(error.c_str())];return;}
 [window center];[window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
}
- (void)fail:(NSString*)message {NSAlert* alert=[[NSAlert alloc]init];alert.messageText=@"Could not open the rebuilt VST3";alert.informativeText=message;[alert runModal];[NSApp terminate:nil];}
- (void)windowDidResize:(NSNotification*)notification {if(host&&window)host->windowSize(window.contentView.bounds.size);}
- (void)windowWillClose:(NSNotification*)notification {host.reset();[NSApp terminate:nil];}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender{return YES;}
- (void)applicationWillTerminate:(NSNotification*)notification {host.reset();}
@end

int main(int argc,char**argv){@autoreleasepool{
 [NSApplication sharedApplication];
 bool selfTest=argc>1&&std::string(argv[1])=="--self-test";
 NSString* bundled=[NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"Virus TI Rework Development.vst3"];
 NSString* path=argc>(selfTest?2:1)?@(argv[selfTest?2:1]):bundled;
 if(selfTest){[NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];PreviewHost host;std::string error;
  if(!host.load(path.UTF8String,error)){std::cerr<<error<<"\n";return 1;}
  NSView* parent=[[NSView alloc]initWithFrame:NSMakeRect(0,0,907,513)];if(!host.attach(parent,error)){std::cerr<<error<<"\n";return 2;}
  if(parent.subviews.count!=1||!host.checkParameterRoundtrip()){std::cerr<<"Editor attach/actual processor edit roundtrip failed\n";return 3;}
  host.close();if(parent.subviews.count){std::cerr<<"Editor detach failed\n";return 4;}
  std::cout<<"PASS: dynamically loaded actual arm64 VST3, initialized component/controller, attached/detached actual IPlugView, UI edit roundtripped through inactive processor; no window/audio/hardware\n";return 0;
 }
 [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
 NSMenu* main=[[NSMenu alloc]init];NSMenuItem* appItem=[[NSMenuItem alloc]init];[main addItem:appItem];NSMenu* menu=[[NSMenu alloc]initWithTitle:@"Virus TI Interface"];
 NSMenuItem* quit=[[NSMenuItem alloc]initWithTitle:@"Quit Virus TI Interface" action:@selector(terminate:) keyEquivalent:@"q"];[menu addItem:quit];appItem.submenu=menu;NSApp.mainMenu=main;
 PreviewApp* delegate=[[PreviewApp alloc]init];delegate->pluginPath=path;NSApp.delegate=delegate;[NSApp run];NSApp.delegate=nil;return 0;
}}
