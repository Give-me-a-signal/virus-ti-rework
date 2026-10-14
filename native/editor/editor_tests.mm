#import <Cocoa/Cocoa.h>
#include "editor.mm"
#include <iostream>
#include <cstdlib>
#define CHECK(...) do{if(!(__VA_ARGS__)){std::cerr<<"FAIL line "<<__LINE__<<": "#__VA_ARGS__<<"\n";std::abort();}}while(0)
class Handler final:public FObject,public IComponentHandler {
public:
 int begins=0,changes=0,ends=0;ParamID last=0;ParamValue value=0;
 tresult PLUGIN_API beginEdit(ParamID id)override{++begins;last=id;return kResultOk;}
 tresult PLUGIN_API performEdit(ParamID id,ParamValue v)override{++changes;last=id;value=v;return kResultOk;}
 tresult PLUGIN_API endEdit(ParamID id)override{++ends;last=id;return kResultOk;}
 tresult PLUGIN_API restartComponent(int32)override{return kResultOk;}
 OBJ_METHODS(Handler,FObject)
 DEFINE_INTERFACES
 DEF_INTERFACE(IComponentHandler)
 END_DEFINE_INTERFACES(FObject)
 REFCOUNT_METHODS(FObject)
};
int main(int argc,char**argv){@autoreleasepool {
 [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
 CHECK(argc==3);NSString* resource=@(argv[1]);NSString* output=@(argv[2]);[[NSFileManager defaultManager]createDirectoryAtPath:output withIntermediateDirectories:YES attributes:nil error:nil];
 Controller c;Handler h;CHECK(c.initialize(nullptr)==kResultOk);c.setComponentHandler(&h);
 VTRNativeEditor* view=[[VTRNativeEditor alloc]initWithController:&c resources:resource];CHECK(view->model);CHECK(view->nodes.count==1236);view->originalUI=NO;[view applyAppearance];
 CHECK([view condition:@"VirusControl/SelectedPage/0 EasyPageSelected" part:0]);
 auto* plug=c.createView(ViewType::kEditor);CHECK(plug);CHECK(plug->isPlatformTypeSupported(kPlatformTypeNSView)==kResultTrue);CHECK(plug->isPlatformTypeSupported("HWND")==kResultFalse);ViewRect size;CHECK(plug->getSize(&size)==kResultTrue);CHECK(size.getWidth()>=907&&size.getWidth()<=1814);CHECK(plug->canResize()==kResultTrue);
 ViewRect constrained{0,0,10,10};CHECK(plug->checkSizeConstraint(&constrained)==kResultTrue);CHECK(constrained.getWidth()==907&&constrained.getHeight()==513);
 constrained={0,0,3000,400};plug->checkSizeConstraint(&constrained);CHECK(constrained.getWidth()==1814&&constrained.getHeight()==1026);
 ViewRect invalidSize{0,0,1000,1000};CHECK(plug->onSize(&invalidSize)==kResultFalse);plug->release();
 NSDictionary* cutoff=nil;NSDictionary* pageTab=nil;NSDictionary* partTab=nil;
 for(NSDictionary* node in view->nodes){NSDictionary* a=node[@"attrs"];if([node[@"type"] containsString:@"Knob"]&&[a[@"Parameter"] isEqual:@"Filter 1/Cutoff"]&&[view visible:node])cutoff=node;
  if([a[@"Parameter"] isEqual:@"VirusControl/SelectedPage"]&&[a[@"OnValue"] intValue]==3&&[view visible:node])pageTab=node;
  if([a[@"Parameter"] isEqual:@"Multi Parameters/Panel Part"]&&[a[@"OnValue"] intValue]==15&&[view visible:node])partTab=node;
 }
 CHECK(cutoff&&pageTab&&partTab);CHECK([view editable:cutoff]);ParamID id=[view paramID:@"Filter 1/Cutoff" part:0];CHECK(id!=kNoParamId);
 [view applyNode:cutoff plain:63 gesture:YES];CHECK(h.begins==1&&h.changes==1&&h.ends==1&&h.last==id);CHECK(c.getParamNormalized(id)==63./127.);CHECK([view plain:@"Filter 1/Cutoff" part:0]==63);
 c.setParamNormalized(id,1);CHECK([view plain:@"Filter 1/Cutoff" part:0]==127); // host automation readback
 [view applyNode:pageTab plain:3 gesture:YES];CHECK(view->selectedPage==3);CHECK(![view condition:@"VirusControl/SelectedPage/0 EasyPageSelected" part:0]);CHECK([view condition:@"VirusControl/SelectedPage/3 FilterPageSelected" part:0]);
 [view applyNode:partTab plain:15 gesture:YES];CHECK(view->selectedPart==15);CHECK([view paramID:@"Filter 1/Cutoff" part:15]!=id);
 [view applyNode:cutoff plain:32 gesture:YES];CHECK(c.getParamNormalized([view paramID:@"Filter 1/Cutoff" part:15])==32./127.);CHECK(c.getParamNormalized(id)==1);
 // Nonautomatable algorithm selectors are explicit local-only per-part state.
 view->selectedPage=2;NSDictionary* mode=view->nodeByID[@"node0473"];CHECK(mode);CHECK([view visible:mode]);CHECK([view editable:mode]);
 CHECK([view paramID:@"Oscillator 1/Mode" part:15]==kNoParamId);
 CHECK([view condition:@"Parameter Active Condition/Oscillator 1/is Classic Oscillator" part:15]);
 const int changesBeforeLocal=h.changes;
 [view applyNode:mode plain:1 gesture:YES];CHECK([view plain:@"Oscillator 1/Mode" part:15]==1);
 CHECK(![view condition:@"Parameter Active Condition/Oscillator 1/is Classic Oscillator" part:15]);CHECK([view condition:@"Parameter Active Condition/Oscillator 1/is HyperSaw Oscillator" part:15]);CHECK([view plain:@"Oscillator 1/Mode" part:0]==0);CHECK(h.changes==changesBeforeLocal);
 CHECK([[view textFor:view->nodeByID[@"node0531"] value:3] isEqual:@"1/16"]);
 // The graph handle must win hit testing over the later masked-knob rectangle.
 view->selectedPage=0;view->selectedPart=0;
 c.setParamNormalized(id,.5);
 NSDictionary* graph=nil;
 for(NSDictionary* node in view->nodes)if([node[@"type"] isEqual:@"LP4Response"]&&[view visible:node]){graph=node;break;}
 CHECK(graph);
 Graphs::Read read=^double(NSString* name){return [view plain:name part:0];};
 auto geometry=Graphs::geometry(graph[@"type"],graph[@"attrs"],rectOf(graph[@"absolute_rect"]),read);
 CHECK(!geometry.handles.empty());NSPoint handle=geometry.handles.front().point;
 CHECK([view nodeAt:handle]==graph);
 auto mouse=[&](NSEventType type,NSPoint p){return [NSEvent mouseEventWithType:type location:[view convertPoint:p toView:nil] modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:1 pressure:1];};
 const int graphChanges=h.changes;
 [view mouseDown:mouse(NSEventTypeLeftMouseDown,handle)];CHECK(view->graphDragging==graph);CHECK(view->graphEditIDs.count>0);
 [view mouseDragged:mouse(NSEventTypeLeftMouseDragged,NSMakePoint(handle.x-8,handle.y-8))];
 [view mouseUp:mouse(NSEventTypeLeftMouseUp,handle)];CHECK(!view->graphDragging);CHECK(h.changes>graphChanges);CHECK(h.begins==h.ends);
 // Bundled banks populate the actual child browser. Partial local recall uses
 // mapped controller edits, selected-part state and explicit patch labeling.
 CHECK(view->browser.bankCount>0);CHECK(view->browser.patchCount>100);
 view->selectedPart=15;
 double untouched=c.getParamNormalized(id);
 [view loadOfflinePatch:@{@"name":@"Test patch",@"verifiedValues":@{@"Filter 1/Cutoff":@42,@"Oscillator 1/Mode":@1}}];
 CHECK([view plain:@"Filter 1/Cutoff" part:15]==42);CHECK(c.getParamNormalized(id)==untouched);
 CHECK([view plain:@"Oscillator 1/Mode" part:15]==1);CHECK([view->patchNames[@15]isEqual:@"Test patch"]);
 // Hardware readback updates the displayed patch without host automation/MIDI.
 NSData* probe=[NSData dataWithContentsOfFile:[[@(argv[1]) stringByDeletingLastPathComponent]stringByAppendingPathComponent:@"Hardware/stock-wavetable-probe.syx"]];
 CHECK(probe.length==524);const auto* bytes=static_cast<const uint8_t*>(probe.bytes);c.hardwarePatch.assign(bytes,bytes+probe.length);for(unsigned part=0;part<16;++part)c.hardwarePatches.parts[part]=virus::patches::ram_single(virus::patches::parse_single(c.hardwarePatch),part);++c.patchRevision;
 int beforeRecall=h.changes;[view synchronizeHardwarePatch];CHECK(h.changes==beforeRecall);CHECK(view->selectedPart==15);CHECK([view->patchNames[@0]length]>0);CHECK([view->patchNames[@15]length]>0);
 // Command-click restores the recovered default through the same host gesture.
 view->selectedPage=0;view->selectedPart=0;c.setParamNormalized(id,.2);
 NSRect knob=rectOf(cutoff[@"absolute_rect"]);NSPoint click=[view convertPoint:NSMakePoint(NSMidX(knob),NSMidY(knob)) toView:nil];
 NSEvent* reset=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:click modifierFlags:NSEventModifierFlagCommand timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:1 pressure:1];
 [view mouseDown:reset];CHECK(c.getParamNormalized(id)==1);
 CGEventRef zeroScroll=CGEventCreateScrollWheelEvent(nullptr,kCGScrollEventUnitLine,1,0);const int beforeScroll=h.changes;[view scrollWheel:[NSEvent eventWithCGEvent:zeroScroll]];CFRelease(zeroScroll);CHECK(h.changes==beforeScroll);
 CHECK(![view nodeAt:NSMakePoint(450,310)]); // background containers are not controls
 CHECK(h.begins==h.ends);
 // The same logical drag at 100% and 200% must produce the same parameter edit.
 NSDictionary* panNode=view->nodeByID[@"node0059"];ParamID panID=[view paramID:@"Patch Common/Panorama" part:0];double draggedValue=-1;
 for(double scale:{1.,2.}){
  [view setEditorSize:NSMakeSize(907*scale,513*scale)];c.setParamNormalized(panID,.5);
  NSRect r=rectOf(panNode[@"absolute_rect"]);NSPoint window=[view convertPoint:NSMakePoint(NSMidX(r),NSMidY(r)) toView:nil];
  auto physicalMouse=[&](NSEventType type,NSPoint pt){return [NSEvent mouseEventWithType:type location:pt modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:1 pressure:1];};
  [view mouseDown:physicalMouse(NSEventTypeLeftMouseDown,window)];CHECK(view->dragging==panNode);
  window.y+=30*scale;[view mouseDragged:physicalMouse(NSEventTypeLeftMouseDragged,window)];[view mouseUp:physicalMouse(NSEventTypeLeftMouseUp,window)];
  double value=c.getParamNormalized(panID);if(draggedValue>=0)CHECK(value==draggedValue);draggedValue=value;
 }
 [view setEditorSize:NSMakeSize(907,513)];view->focused=panNode;
 int beforeKey=h.changes;double beforeValue=c.getParamNormalized(panID);
 NSEvent* arrow=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:nil characters:@"" charactersIgnoringModifiers:@"" isARepeat:NO keyCode:124];
 [view keyDown:arrow];CHECK(h.changes==beforeKey+1&&c.getParamNormalized(panID)>beforeValue);CHECK(h.begins==h.ends);
 // Each sidebar CC7 control targets its own part, independent of the editor's
 // selected part. Horizontal physical dragging scales consistently in both skins.
 NSString* volumeName=@"Internal MIDI Standard Parameters/07 Volume";
 NSMutableArray<NSDictionary*>* strips=[NSMutableArray array];for(NSDictionary* node in view->nodes)if([node[@"type"]isEqual:@"VUSlider"])[strips addObject:node];CHECK(strips.count==16);
 for(BOOL original:{NO,YES})for(double scale:{1.,2.}){
  view->originalUI=original;[view applyAppearance];[view setEditorSize:NSMakeSize(907*scale,513*scale)];view->selectedPart=15;
  for(NSInteger part=0;part<16;++part){NSDictionary* strip=strips[part];CHECK([view partOf:strip]==part&&[view editable:strip]);
   ParamID volumeID=[view paramID:volumeName part:part];CHECK(volumeID!=kNoParamId);c.setParamNormalized(volumeID,64./127.);
   std::array<double,16> previous;for(int p=0;p<16;++p)previous[p]=c.getParamNormalized([view paramID:volumeName part:p]);
   NSRect r=rectOf(strip[@"absolute_rect"]);NSPoint start=[view convertPoint:NSMakePoint(NSMidX(r),NSMidY(r)) toView:nil];
   auto event=[&](NSEventType type,NSPoint at){return [NSEvent mouseEventWithType:type location:at modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:1 pressure:1];};
   int begin=h.begins,end=h.ends,changes=h.changes;
   [view mouseDown:event(NSEventTypeLeftMouseDown,start)];CHECK(view->dragging==strip&&view->dragID==volumeID);
   NSPoint finish=NSMakePoint(start.x+20*scale,start.y+37*scale);[view mouseDragged:event(NSEventTypeLeftMouseDragged,finish)];[view mouseUp:event(NSEventTypeLeftMouseUp,finish)];
   CHECK(h.begins==begin+1&&h.ends==end+1&&h.changes==changes+1&&h.last==volumeID);CHECK([view plain:volumeName part:part]==(original?93:89));CHECK(view->selectedPart==15);
   for(int p=0;p<16;++p)if(p!=part)CHECK(c.getParamNormalized([view paramID:volumeName part:p])==previous[p]);
  }
 }
 [view setEditorSize:NSMakeSize(907,513)];view->focused=nil;
 // Rendered activity and CC7 handle must respond independently. Rendering has
 // no parameter edits; fixture activity is telemetry, not a change to the sound.
 auto renderStrip=[&](NSDictionary* strip){NSImage* image=[[NSImage alloc]initWithSize:NSMakeSize(907,513)];[image lockFocusFlipped:YES];[NSColor.clearColor setFill];NSRectFill(NSMakeRect(0,0,907,513));if(view->originalUI)[view drawNode:strip];else[view drawModernNode:strip];[image unlockFocus];return image.TIFFRepresentation;};
 for(BOOL original:{NO,YES}){
  view->originalUI=original;[view applyAppearance];ParamID volumeID=[view paramID:volumeName part:0];c.setParamNormalized(volumeID,0);c.hardwareMeters.midiActivity[0]=0;int unchanged=h.changes;
  NSData* idle=renderStrip(strips[0]);c.hardwareMeters.midiActivity[0]=1;NSData* active=renderStrip(strips[0]);CHECK(![idle isEqualToData:active]);CHECK([view plain:volumeName part:0]==0);
  c.setParamNormalized(volumeID,1);NSData* louder=renderStrip(strips[0]);CHECK(![active isEqualToData:louder]);CHECK(c.hardwareMeters.midiActivity[0]==1);CHECK(h.changes==unchanged);
 }
 // Deliberate synthetic telemetry for the rendered page fixtures, not a capture.
 c.hardwareMeters.usbPeak={.5f,.125f,.0316228f};for(unsigned part=0;part<16;++part)c.hardwareMeters.midiActivity[part]=part%4==0?0.f:float(part+1)/16.f;
 auto metersBefore=c.hardwareMeters;
 view->originalUI=NO;[view applyAppearance];
 int beforeTheme=h.changes;view->darkTheme=YES;[view applyAppearance];view->darkTheme=NO;[view applyAppearance];CHECK(h.changes==beforeTheme);view->focused=nil;
 // Switching renderers preserves synth, navigation, browser and zoom state.
 NSUserDefaults* appearancePrefs=[[NSUserDefaults alloc]initWithSuiteName:@"org.virus-ti-rework.editor"];NSObject* savedOriginal=[appearancePrefs objectForKey:@"originalUI"];
 view->selectedPart=9;view->selectedPage=3;view->darkTheme=NO;[view setEditorSize:NSMakeSize(1134,641)];
 NSDictionary* localBefore=[view->localValues copy];NSDictionary* namesBefore=[view->patchNames copy];auto patchesBefore=c.hardwarePatches;
 std::vector<double> paramsBefore;for(const auto& spec:kCatalog)paramsBefore.push_back(c.getParamNormalized(spec.id));
 auto browserBefore=view->browser;NSArray* countsBefore=browserBefore.filteredPatchCounts;NSString* statusBefore=browserBefore.statusText;NSSize frameBefore=view.frame.size;int editsBeforeStyle=h.changes;
 for(NSInteger style:{1,0,1}){
  [view->uiStyleButton selectItemWithTag:style];[view changeUIStyle:view->uiStyleButton];CHECK(view->originalUI==(style==1));CHECK(view->themeButton.enabled==(style==0));
  CHECK([appearancePrefs boolForKey:@"originalUI"]==(style==1));CHECK(!view->darkTheme);CHECK(view->selectedPart==9&&view->selectedPage==3);CHECK(NSEqualSizes(frameBefore,view.frame.size));
  CHECK(view->browser==browserBefore&&[countsBefore isEqual:browserBefore.filteredPatchCounts]&&[statusBefore isEqual:browserBefore.statusText]);
  CHECK([localBefore isEqual:view->localValues]&&[namesBefore isEqual:view->patchNames]);CHECK(h.changes==editsBeforeStyle&&h.begins==h.ends);
  std::size_t n=0;for(const auto& spec:kCatalog)CHECK(paramsBefore[n++]==c.getParamNormalized(spec.id));
  CHECK(c.hardwarePatches.parts==patchesBefore.parts);CHECK(c.hardwareMeters.midiActivity==metersBefore.midiActivity&&c.hardwareMeters.usbPeak==metersBefore.usbPeak);
 }
 VTRNativeEditor* restoredStyle=[[VTRNativeEditor alloc]initWithController:&c resources:resource];CHECK(restoredStyle->originalUI);CHECK(restoredStyle->uiStyleButton.selectedTag==1);restoredStyle=nil;
 if(savedOriginal)[appearancePrefs setObject:savedOriginal forKey:@"originalUI"];else[appearancePrefs removeObjectForKey:@"originalUI"];
 view->originalUI=NO;[view applyAppearance];
 // Scaling preserves the recovered coordinate space and hit targets on every page.
 for(double scale:{1.,1.25,1.5,2.}){
  [view setEditorSize:NSMakeSize(std::round(907*scale),std::round(513*scale))];CHECK(std::abs(view.bounds.size.width-907)<1e-5&&std::abs(view.bounds.size.height-513)<1e-5);
  for(NSNumber* page in @[@0,@2,@3,@4,@5,@6,@7,@9,@11]){view->selectedPage=page.integerValue;
   for(NSDictionary* node in view->nodes)if([view visible:node]&&[view editable:node]){NSRect r=rectOf(node[@"absolute_rect"]);NSPoint q=NSMakePoint(NSMidX(r),NSMidY(r));NSPoint window=[view convertPoint:q toView:nil];NSPoint back=[view convertPoint:window fromView:nil];CHECK(std::abs(back.x-q.x)<1e-6&&std::abs(back.y-q.y)<1e-6);}
  }
 }
 [view setEditorSize:NSMakeSize(907,513)];
 for(NSString* style in @[@"dark",@"polar",@"original"])for(NSNumber* page in @[@0,@1,@2,@3,@4,@5,@6,@7,@9,@11,@8,@12]){
  view->originalUI=[style isEqual:@"original"];view->darkTheme=![style isEqual:@"polar"];[view applyAppearance];
  view->selectedPage=page.integerValue;view->selectedPart=0;view->selectedSection=(page.intValue==8||page.intValue==12)?1:0;view->selectedUtility=page.intValue==12?1:0;
  [view updateBrowserVisibility];[view layoutSubtreeIfNeeded];
  NSUInteger visible=0;for(NSDictionary* node in view->nodes)if([view visible:node])++visible;CHECK(visible>35);
  NSImage* rendered=[[NSImage alloc]initWithSize:NSMakeSize(907,513)];[rendered lockFocusFlipped:YES];[view displayRectIgnoringOpacity:view.bounds inContext:NSGraphicsContext.currentContext];[rendered unlockFocus];
  NSBitmapImageRep* rep=[[NSBitmapImageRep alloc]initWithData:rendered.TIFFRepresentation];CHECK((rep.pixelsWide==907&&rep.pixelsHigh==513)||(rep.pixelsWide==1814&&rep.pixelsHigh==1026));NSData* png=[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];CHECK([png writeToFile:[output stringByAppendingPathComponent:[NSString stringWithFormat:@"%@-page-%02d.png",style,page.intValue]] atomically:YES]);
  std::cout<<"page "<<page.intValue<<": "<<visible<<" visible nodes\n";
 }
 CHECK(c.hardwareMeters.midiActivity==metersBefore.midiActivity&&c.hardwareMeters.usbPeak==metersBefore.usbPeak);
 view=nil;c.setComponentHandler(nullptr);c.terminate();std::cout<<"PASS native Cocoa editor: VST3 view contract, parameter gestures, host readback, 16-part selection, named conditions, 36 page/style renders, persistent renderer switching without synth edits, independent MIDI activity/CC7 rendering and all-16 CC7 gestures; no window or host launched\n";
 }return 0;}
