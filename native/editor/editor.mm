#import <Cocoa/Cocoa.h>
#include "editor.h"
#include "builtin_model.h"
#include "appearance_import.h"
#include "graphs.h"
#include "browser.h"
#include "browser_bank.hpp"
#include "bitmap_layout.h"
#include "modern_theme.h"
#include "illustrative_waves.h"
#include "plugin.h"
#include "public.sdk/source/common/pluginview.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <string>
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace VirusRework;

@interface VTRNativeEditor : NSView {
@public
 Controller* controller;
 NSDictionary* model;
 NSArray* nodes;
 NSDictionary* parameters;
 NSDictionary* conditions;
 NSMutableDictionary* images;
 NSMutableDictionary* localValues;
 NSMutableDictionary* nodeByID;
 NSString* resourceDirectory;
 NSDictionary* dragging;
 NSDictionary* hovered;
 NSDictionary* focused;
 NSDictionary* graphDragging;
 NSInteger graphHandle;
 NSInteger graphPart;
 NSMutableSet<NSNumber*>* graphEditIDs;
 VTRPatchBrowserView* browser;
 NSMutableDictionary<NSNumber*,NSString*>* patchNames;
 NSTrackingArea* tracking;
 NSPoint dragStart;
 double dragValue;
 ParamID dragID;
 NSInteger selectedPart;
 NSInteger selectedPage;
 NSInteger selectedSection;
 NSInteger selectedUtility;
 NSTimer* refresh;
 uint64_t displayedPatchRevision;
 uint64_t displayedParameterRevision;
 BOOL refreshInitialized;
 BOOL displayedConnected;
 std::string displayedHardwareText;
 MeterSnapshot displayedMeters;
 BOOL suppressPresetAutomation;
 NSButton* connectButton;
 NSButton* disconnectButton;
 NSButton* testButton;
 NSButton* themeButton;
 NSPopUpButton* scaleButton;
 NSPopUpButton* uiStyleButton;
 BOOL originalUI;
 BOOL darkTheme;
 void (^resizeHandler)(double);
 std::unordered_map<std::string,ParamID> bindings;
 std::unordered_set<ParamID> editableBindings;
}
- (instancetype)initWithController:(Controller*)value resources:(NSString*)directory;
- (BOOL)condition:(NSString*)name part:(NSInteger)part;
- (BOOL)visible:(NSDictionary*)node;
- (double)plain:(NSString*)name part:(NSInteger)part;
- (void)drawNode:(NSDictionary*)node;
- (void)applyNode:(NSDictionary*)node plain:(double)value gesture:(BOOL)gesture;
- (NSDictionary*)nodeAt:(NSPoint)point;
- (BOOL)point:(NSPoint)point insideNode:(NSDictionary*)node;
- (void)selectMenu:(NSMenuItem*)sender;
- (void)writeParameter:(NSString*)name part:(NSInteger)part value:(double)value gesture:(BOOL)gesture;
- (void)loadOfflinePatch:(NSDictionary*)patch;
- (void)updateBrowserVisibility;
- (void)finishGraphGesture;
- (void)refreshEditor;
- (void)hardwareAction:(NSButton*)sender;
- (void)drawModernNode:(NSDictionary*)node;
- (void)drawModernPanels;
- (void)applyAppearance;
- (void)changeUIStyle:(NSPopUpButton*)sender;
- (void)setEditorSize:(NSSize)size;
@end

namespace {
NSRect rectOf(NSDictionary* rect) {return NSMakeRect([rect[@"left"] doubleValue],[rect[@"top"] doubleValue],[rect[@"width"] doubleValue],[rect[@"height"] doubleValue]);}
NSString* bindingKey(NSString* name,NSInteger part) {return [NSString stringWithFormat:@"%ld:%@",(long)part,name];}
}
@implementation VTRNativeEditor
- (BOOL)isFlipped {return YES;}
- (BOOL)acceptsFirstResponder {return YES;}
- (BOOL)acceptsFirstMouse:(NSEvent*)event {(void)event;return YES;}
- (instancetype)initWithController:(Controller*)value resources:(NSString*)directory {
 if((self=[super initWithFrame:NSMakeRect(0,0,907,513)])) {
  controller=value;resourceDirectory=directory;images=[NSMutableDictionary dictionary];localValues=[NSMutableDictionary dictionary];nodeByID=[NSMutableDictionary dictionary];
  model=builtinEditorModel();resourceDirectory=originalAppearanceDirectory();
  nodes=model[@"nodes"]?:@[];parameters=model[@"parameters"]?:@{};conditions=model[@"conditions"]?:@{};
  connectButton=[NSButton buttonWithTitle:@"Connect TI" target:self action:@selector(hardwareAction:)];connectButton.tag=1;connectButton.frame=NSMakeRect(5,487,90,22);[self addSubview:connectButton];
  disconnectButton=[NSButton buttonWithTitle:@"Disconnect" target:self action:@selector(hardwareAction:)];disconnectButton.tag=2;disconnectButton.frame=NSMakeRect(96,487,94,22);[self addSubview:disconnectButton];
  testButton=[NSButton buttonWithTitle:@"Test tone" target:self action:@selector(hardwareAction:)];testButton.tag=3;testButton.frame=NSMakeRect(792,487,108,22);[self addSubview:testButton];
  connectButton.frame=NSMakeRect(10,487,78,20);disconnectButton.frame=NSMakeRect(92,487,78,20);testButton.frame=NSMakeRect(174,487,72,20);testButton.hidden=YES;
  themeButton=[NSButton buttonWithTitle:@"Dark" target:self action:@selector(changeTheme:)];themeButton.frame=NSMakeRect(686,487,91,20);[self addSubview:themeButton];
  scaleButton=[[NSPopUpButton alloc]initWithFrame:NSMakeRect(789,486,104,22) pullsDown:NO];for(NSNumber* zoom in @[@100,@125,@150,@175,@200]){[scaleButton addItemWithTitle:[NSString stringWithFormat:@"%@%%",zoom]];scaleButton.lastItem.tag=zoom.integerValue;}scaleButton.target=self;scaleButton.action=@selector(changeScale:);[self addSubview:scaleButton];
  for(NSButton* button in @[connectButton,disconnectButton,testButton,themeButton]){button.font=[NSFont systemFontOfSize:10 weight:NSFontWeightMedium];button.bezelStyle=NSBezelStyleRounded;}
  uiStyleButton=[[NSPopUpButton alloc]initWithFrame:NSMakeRect(548,486,132,22) pullsDown:NO];[uiStyleButton addItemWithTitle:@"Modern UI"];uiStyleButton.lastItem.tag=0;[uiStyleButton addItemWithTitle:@"Original Access"];uiStyleButton.lastItem.tag=1;[uiStyleButton addItemWithTitle:@"Import Original…"];uiStyleButton.lastItem.tag=2;uiStyleButton.font=[NSFont systemFontOfSize:10];uiStyleButton.target=self;uiStyleButton.action=@selector(changeUIStyle:);uiStyleButton.toolTip=@"Choose the modern editor or the reconstructed original Access artwork";[self addSubview:uiStyleButton];
  NSUserDefaults* appearancePrefs=[[NSUserDefaults alloc]initWithSuiteName:@"org.virus-ti-rework.editor"];
  originalUI=!getenv("VTR_EDITOR_TEST_DEFAULTS")&&[[appearancePrefs stringForKey:@"appearance"] isEqual:@"original"];
  if(originalUI){NSDictionary* imported=originalAppearanceModel(originalAppearanceDirectory(),nullptr);if(imported){model=imported;nodes=model[@"nodes"];parameters=model[@"parameters"];conditions=model[@"conditions"];}else originalUI=NO;}
  themeButton.toolTip=@"Switch between Virus Dark and Polar light themes";scaleButton.toolTip=@"Editor scale — the layout stays proportional";
  darkTheme=getenv("VTR_EDITOR_TEST_DEFAULTS")||![[[NSUserDefaults alloc]initWithSuiteName:@"org.virus-ti-rework.editor"]boolForKey:@"polarTheme"];
  selectedPart=0;selectedPage=0;selectedSection=0;selectedUtility=0;dragID=kNoParamId;graphHandle=-1;
  graphEditIDs=[NSMutableSet set];patchNames=[NSMutableDictionary dictionary];
  for(NSDictionary* node in nodes)if(node[@"id"])nodeByID[node[@"id"]]=node;
  tracking=[[NSTrackingArea alloc]initWithRect:self.bounds options:NSTrackingMouseMoved|NSTrackingMouseEnteredAndExited|NSTrackingActiveInKeyWindow|NSTrackingInVisibleRect owner:self userInfo:nil];[self addTrackingArea:tracking];
  for(const auto& spec:kCatalog){bindings[std::string([bindingKey(@(spec.name),spec.part-1) UTF8String])]=spec.id;
   if(spec.midiCC>=0&&spec.resolved&&!std::strstr(spec.name,"Wavetable")&&!std::strstr(spec.name,"HyperSaw")&&!std::strstr(spec.name,"Vocoder"))editableBindings.insert(spec.id);
  }
  if(model){
   __weak VTRNativeEditor* weak=self;
   NSString* banks=[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/VirusTIRework/Patches"];
   browser=[[VTRPatchBrowserView alloc]initWithFrame:NSMakeRect(192,45,705,426) resources:directory bankDirectory:banks selectionHandler:^(NSDictionary* patch){[weak recallPatch:patch];}];
   browser.hidden=YES;[self addSubview:browser];
   [self applyAppearance];
  }
 }
 return self;
}
- (void)viewDidMoveToWindow {
 [refresh invalidate];refresh=nil;
 if(self.window){__weak VTRNativeEditor* weak=self;refresh=[NSTimer scheduledTimerWithTimeInterval:0.1 repeats:YES block:^(NSTimer*){VTRNativeEditor* strong=weak;if(strong&&!strong.hiddenOrHasHiddenAncestor&&(strong.window.occlusionState&NSWindowOcclusionStateVisible))[strong refreshEditor];}];refresh.tolerance=.02;}
}
- (void)refreshEditor {
 if(originalUI&&![[NSFileManager defaultManager]fileExistsAtPath:[originalAppearanceDirectory() stringByAppendingPathComponent:@"editor.xml"]]){[uiStyleButton selectItemWithTag:0];[self changeUIStyle:uiStyleButton];}
 controller->hardwareCommand(0);
 BOOL patchChanged=displayedPatchRevision!=controller->patchRevision;
 [self synchronizeHardwarePatch];
 // A heartbeat is not a visual change. Host automation and real patch readback
 // invalidate the page, while meter decay only invalidates its small strips.
 BOOL full=!refreshInitialized||patchChanged||displayedParameterRevision!=controller->editorParameterRevision;
 if(full)[self setNeedsDisplay:YES];
 else {
  if(displayedConnected!=controller->hardwareConnected||displayedHardwareText!=controller->hardwareText)[self setNeedsDisplayInRect:NSMakeRect(258,487,281,22)];
  for(unsigned part=0;part<16;++part)if(displayedMeters.midiActivity[part]!=controller->hardwareMeters.midiActivity[part])for(NSDictionary* node in nodes)if([node[@"type"]isEqual:@"VUSlider"]&&[self partOf:node]==part&&[self visible:node])[self setNeedsDisplayInRect:rectOf(node[@"absolute_rect"])];
  for(unsigned bus=0;bus<3;++bus)if(displayedMeters.usbPeak[bus]!=controller->hardwareMeters.usbPeak[bus])[self setNeedsDisplayInRect:NSMakeRect(652+bus*79,469,74,14)];
 }
 refreshInitialized=YES;displayedParameterRevision=controller->editorParameterRevision;displayedConnected=controller->hardwareConnected;displayedHardwareText=controller->hardwareText;displayedMeters=controller->hardwareMeters;
}
- (void)hardwareAction:(NSButton*)sender {controller->hardwareCommand(int(sender.tag));}
- (void)dealloc { [refresh invalidate];if(dragID!=kNoParamId)controller->endEdit(dragID);for(NSNumber* identifier in graphEditIDs)controller->endEdit(identifier.unsignedIntValue); }
- (NSInteger)partOf:(NSDictionary*)node {NSInteger part=[node[@"attrs"][@"Part"] integerValue];return part<0?selectedPart:part;}
- (ParamID)paramID:(NSString*)name part:(NSInteger)part {
 if(!name)return kNoParamId;auto it=bindings.find([bindingKey(name,part) UTF8String]);return it==bindings.end()?kNoParamId:it->second;
}
- (NSDictionary*)definition:(NSString*)name {NSDictionary* p=parameters[name];return p[@"attrs"]?:p;}
- (double)plain:(NSString*)name part:(NSInteger)part {
 if([name isEqual:@"VirusControl/SelectedPage"])return selectedPage;
 if([name isEqual:@"Multi Parameters/Panel Part"])return selectedPart;
 if([name isEqual:@"VirusControl/SelectedUtilityPage"])return selectedUtility;
 if([name isEqual:@"VirusControl/Selected Section"])return selectedSection;
 if([name isEqual:@"VirusControl/RecallProgress"]||[name isEqual:@"VirusControl/InitialSyncProgress"])return 127;
 if([name isEqual:@"SystemStatus/Error"]||[name isEqual:@"Internal System Info/Display Welcome Page"])return 0;
 auto id=[self paramID:name part:part];if(id!=kNoParamId)return controller->normalizedParamToPlain(id,controller->getParamNormalized(id));
 NSNumber* local=localValues[bindingKey(name,part)];if(local)return local.doubleValue;
 NSDictionary* d=[self definition:name];return [d[@"defaultValue"] doubleValue];
}
- (BOOL)evaluate:(NSDictionary*)expr part:(NSInteger)part {
 NSString* op=expr[@"op"];
 if([op isEqual:@"not"])return ![self evaluate:expr[@"arg"] part:part];
 if([op isEqual:@"and"]||[op isEqual:@"or"]){BOOL isAnd=[op isEqual:@"and"];for(NSDictionary* arg in expr[@"args"]){BOOL x=[self evaluate:arg part:part];if(x!=isAnd)return !isAnd;}return isAnd;}
 if([op isEqual:@"test"]){double value=[self plain:expr[@"parameter"] part:part];double lo=[expr[@"min"] doubleValue],hi=[expr[@"max"] doubleValue];NSString* type=expr[@"type"];
  if([type isEqual:@"equals"]||[type isEqual:@"equal"])return value==lo;
  if([type isEqual:@"inside"])return value>=lo&&value<=hi;
  if([type isEqual:@"outside"])return value<lo||value>hi;
 }return NO;
}
- (BOOL)condition:(NSString*)name part:(NSInteger)part {
 if(!name.length)return YES;
 // Explicit offline-editor display policy. These select UI layers only; they
 // do not modify or attest any physical-device or system-status parameter.
 if([name isEqual:@"Internal System Info/No Communication Error"])return YES;
 if([name isEqual:@"Internal System Info/Any Communication Error"])return NO;
 if([name hasPrefix:@"SystemStatus/"])return NO;
 if([name hasPrefix:@"Internal System Info/Display "])return NO;
 if([name hasPrefix:@"Locks/"])return [name containsString:@"No Lockings"];
 NSDictionary* condition=conditions[name];NSDictionary* expression=condition[@"expression"]?:condition[@"ast"];
 if(expression)return [self evaluate:expression part:part];
 return NO; // Unknown conditions never expose contradictory overlaid controls.
}
- (BOOL)visible:(NSDictionary*)node {
 NSInteger part=[self partOf:node];
 NSInteger page=model[@"classic_layout"]&&selectedSection?(selectedUtility?12:8):selectedPage;
 if(node[@"page"]&&[node[@"page"]integerValue]>=0&&[node[@"page"]integerValue]!=page)return NO;
 if(![self condition:node[@"attrs"][@"Condition"] part:part])return NO;
 id parent=node[@"parent_id"];
 if(parent&&parent!=[NSNull null]){NSDictionary* n=nodeByID[parent];if(n)return [self visible:n];}
 return YES;
}
- (NSImage*)image:(NSString*)name {
 if(!name.length)return nil;NSImage* image=images[name];if(!image){image=Artwork::load([resourceDirectory stringByAppendingPathComponent:name]);if(image)images[name]=image;}return image;
}
- (void)bitmapText:(NSString*)text font:(NSString*)font rect:(NSRect)rect align:(NSString*)align {
 NSImage* atlas=[self image:font];
 if(atlas&&atlas.size.width<=16&&atlas.size.height>=500){CGFloat w=atlas.size.width,h=atlas.size.height/96;CGFloat x=rect.origin.x;if([align containsString:@"Center"])x+=std::max(0.,(rect.size.width-text.length*w)/2);else if([align containsString:@"Right"])x+=std::max(0.,rect.size.width-text.length*w);CGFloat y=rect.origin.y+std::max(0.,(rect.size.height-h)/2);
  for(NSUInteger i=0;i<text.length&&x+w<=NSMaxX(rect);++i,x+=w){unichar ch=[text characterAtIndex:i];if(ch<32||ch>127)ch='?';NSRect source=NSMakeRect(0,atlas.size.height-(ch-32+1)*h,w,h);[atlas drawInRect:NSMakeRect(x,y,w,h) fromRect:source operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];}return;
 }
 [text drawInRect:rect withAttributes:@{NSFontAttributeName:[NSFont monospacedSystemFontOfSize:10 weight:NSFontWeightRegular],NSForegroundColorAttributeName:NSColor.whiteColor}];
}
- (NSString*)textFor:(NSDictionary*)node value:(double)value {
 NSString* name=node[@"attrs"][@"Parameter"];NSDictionary* definition=[self definition:name];NSString* list=definition[@"valuelist"];
 id values=model[@"valuelists"][list];
 if([values isKindOfClass:NSDictionary.class])values=values[@"values"]?:values[@"entries"];
 if([values isKindOfClass:NSArray.class])for(id item in values){if([item isKindOfClass:NSDictionary.class]){NSDictionary* a=item[@"attrs"]?:item;if([(a[@"value"]?:a[@"index"]) doubleValue]==value)return item[@"text"]?:a[@"name"]?:a[@"text"]?:a[@"label"]?:[NSString stringWithFormat:@"%.0f",value];}}
 if(name.length)return definition.count?[NSString stringWithFormat:@"%.0f",value]:@"";
 return node[@"attrs"][@"Name"]?:@"";
}
- (void)drawNode:(NSDictionary*)node {
 if(![self visible:node])return;
 if([node[@"attrs"][@"Parameter"] isEqual:@"LockView/Part Lock Indicator"])return;
 NSDictionary* a=node[@"attrs"];NSString* type=node[@"type"];NSRect rect=rectOf(node[@"absolute_rect"]);
 if(rect.size.width<=0||rect.size.height<=0)return;
 if(![self needsToDrawRect:rect])return;
 NSString* parameter=a[@"Parameter"];NSInteger part=[self partOf:node];double value=[self plain:parameter part:part];NSDictionary* definition=[self definition:parameter];double minimum=[definition[@"minValue"] doubleValue],maximum=[definition[@"maxValue"] doubleValue];double normalized=maximum>minimum?std::clamp((value-minimum)/(maximum-minimum),0.,1.):0.;
 NSString* imageName=a[@"NormalBack"];
 if([type isEqual:@"RadioButton"]&&value==[a[@"OnValue"] doubleValue]&&a[@"PressedBack"])imageName=a[@"PressedBack"];
 else if([type isEqual:@"Button"]&&value!=minimum&&a[@"PressedBack"])imageName=a[@"PressedBack"];
 NSImage* image=[self image:imageName];
 NSEdgeInsets cap=Artwork::insets(model[@"bitmaps"][imageName]);
 [NSGraphicsContext saveGraphicsState];NSRectClip(rect);
 id ancestor=node[@"parent_id"];
 while(ancestor&&ancestor!=[NSNull null]){NSDictionary* parent=nodeByID[ancestor];if(!parent)break;NSRectClip(rectOf(parent[@"absolute_rect"]));ancestor=parent[@"parent_id"];}

 if(image){NSSize size=image.size;NSRect source=NSMakeRect(0,0,size.width,size.height);
  // XML control height identifies frames in vertical strips. Horizontal menus
  // stretch their single-frame background to the explicitly declared geometry.
  NSInteger frames=static_cast<NSInteger>(std::round(size.height/rect.size.height));
  if(frames>1&&std::abs(size.height-frames*rect.size.height)<.1){double n=[type isEqual:@"VUSlider"]?controller->hardwareMeters.midiActivity[std::clamp(part,0L,15L)]:normalized;if([type isEqual:@"RadioButton"])n=value==[a[@"OnValue"] doubleValue]?1:0;NSInteger index=std::lround(n*(frames-1));source=NSMakeRect(0,size.height-(index+1)*rect.size.height,size.width,rect.size.height);}
  if(!Artwork::sliced(cap)&&source.size.width>rect.size.width)source.size.width=rect.size.width;
  Artwork::draw(image,rect,source,cap);
 }
 if([type isEqual:@"VUSlider"]){NSImage* handle=[self image:a[@"NormalHandle"]];if(handle){NSSize size=handle.size;NSRect target=NSMakeRect(rect.origin.x+normalized*(rect.size.width-size.width),rect.origin.y,size.width,rect.size.height);[handle drawInRect:target fromRect:NSMakeRect(0,0,size.width,size.height) operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];}}
 if(Graphs::supports(type))Graphs::draw(type,a,rect,^double(NSString* name){return [self plain:name part:part];},^NSImage*(NSString* name){return [self image:name];});
 if([type isEqual:@"StaticText"]||([type containsString:@"Menu"]&&![type isEqual:@"PatchSaveMenu"])||[type isEqual:@"TextSlider"]||[type isEqual:@"NameEdit"]){NSString* text=[self textFor:node value:value];
  if([type isEqual:@"NameEdit"])text=patchNames[@(part)]?:@"Offline";
  BOOL menu=[type containsString:@"Menu"];
  NSRect textRect=menu?Artwork::menuTextRect(rect,cap):NSInsetRect(rect,2,0);
  if(menu){NSUInteger capacity=static_cast<NSUInteger>(std::max(0.,std::floor(textRect.size.width/6.)));if(text.length>capacity)text=capacity>=3?[[text substringToIndex:capacity-3]stringByAppendingString:@"..."]:[text substringToIndex:capacity];}
  [self bitmapText:text font:a[@"ControlFont"] rect:textRect align:menu?@"Middle Center":a[@"Align"]?:@""];
 }
 if([type isEqual:@"PatchBrowser"]||[type isEqual:@"RemoteBrowser"]||[type isEqual:@"ArpEdit"]){
  [[NSColor colorWithWhite:.12 alpha:.9]setFill];NSRectFill(rect);
  NSString* note=[type isEqual:@"ArpEdit"]?@"Pattern editor reconstruction pending":@"Library / hardware data unavailable offline";
  [note drawInRect:NSInsetRect(rect,12,12) withAttributes:@{NSFontAttributeName:[NSFont systemFontOfSize:11],NSForegroundColorAttributeName:NSColor.lightGrayColor}];
 }
 if(parameter.length&&![self editable:node]&&([type containsString:@"Knob"]||[type containsString:@"Menu"]||[type containsString:@"Button"]||[type containsString:@"Slider"])){[[NSColor colorWithWhite:.45 alpha:.38]setFill];NSRectFillUsingOperation(rect,NSCompositingOperationSourceOver);}
 [NSGraphicsContext restoreGraphicsState];
}
#include "modern_editor.inc"
- (void)drawRect:(NSRect)dirty {
 [self updateBrowserVisibility];
 if(originalUI){[[NSColor colorWithWhite:.15 alpha:1]setFill];NSRectFill(self.bounds);for(NSDictionary* node in nodes)[self drawNode:node];}
 else{[self drawModernPanels];for(NSDictionary* node in nodes)[self drawModernNode:node];}
 if(originalUI)Skin::box(NSMakeRect(0,470,907,43),Skin::Palette(true).base,nil,0);
 if(focused&&[self visible:focused]){NSRect r=NSInsetRect(rectOf(focused[@"absolute_rect"]),-2,-2);auto ring=[NSBezierPath bezierPathWithRoundedRect:r xRadius:3 yRadius:3];[Skin::Palette(originalUI||darkTheme).accent setStroke];CGFloat dash[2]={2,2};[ring setLineDash:dash count:2 phase:0];ring.lineWidth=.8;[ring stroke];}
 Skin::Palette p(originalUI||darkTheme);Skin::line(NSMakePoint(10,483),NSMakePoint(897,483),p.line,.7);
 NSString* message=@(controller->hardwareText.c_str());
 if([message hasPrefix:@"Disconnected"]&&![message containsString:@"attention"])message=@"Offline · Presets and editing available";
 Skin::text(message,NSMakeRect(258,488,281,18),9,p.muted);
 Skin::text(@"MIDI activity  /  CC7 volume",NSMakeRect(12,471,177,11),7,p.muted,YES);
 if(!hovered&&!originalUI&&model[@"classic_layout"])Skin::text(@"Dimmed controls unavailable · Graphs illustrative",NSMakeRect(211,470,431,12),8,p.muted);
 for(unsigned bus=0;bus<3;++bus){float level=controller->hardwareMeters.usbPeak[bus];CGFloat x=652+bus*79;NSString* db=level>1e-6?[NSString stringWithFormat:@"%.0f dBFS",20*log10(level)]:@"−∞ dBFS";Skin::text([NSString stringWithFormat:@"USB%u  %@",bus+1,db],NSMakeRect(x,469,74,10),6.5,p.muted,YES);Skin::box(NSMakeRect(x,480,74,2),p.line,nil,1);Skin::box(NSMakeRect(x,480,74*meterFraction(level),2),level>=1?NSColor.systemRedColor:p.accent,nil,1);}

 if(hovered){NSString* name=hovered[@"attrs"][@"Parameter"];NSInteger part=[self partOf:hovered];
  BOOL mapped=[self paramID:name part:part]!=kNoParamId;NSString* suffix=[self editable:hovered]?(mapped?@"Shift: fine  ·  Double-click: reset":@"Local control · not stored in the sound"):@"Read-only";
  if([hovered[@"type"]isEqual:@"VUSlider"])suffix=@"CC7 volume · fill shows MIDI activity, not dB";
  Skin::text([NSString stringWithFormat:@"%@  ·  %@   —   %@",name,[self textFor:hovered value:[self plain:name part:part]],suffix],NSMakeRect(211,470,431,12),8,p.muted);
 }
}
- (BOOL)editable:(NSDictionary*)node {
 NSDictionary* a=node[@"attrs"];NSString* name=a[@"Parameter"];NSString* type=node[@"type"];
 if(!Graphs::supports(type)&&![type containsString:@"Knob"]&&![type containsString:@"Slider"]&&![type containsString:@"Menu"]&&![type containsString:@"Button"])return NO;
 if([type isEqual:@"PatchSaveMenu"])return NO;
 if([a[@"Enabled"] isEqual:@"false"])return NO;
 id ancestor=node[@"parent_id"];
 while(ancestor&&ancestor!=[NSNull null]){NSDictionary* parent=nodeByID[ancestor];if(!parent)break;if([parent[@"attrs"][@"Enabled"] isEqual:@"false"])return NO;ancestor=parent[@"parent_id"];}
 if(Graphs::supports(type))return NO; // Illustrative beta graphs do not imply hardware support.
 if([name isEqual:@"VirusControl/Selected Section"]||[name isEqual:@"VirusControl/SelectedPage"]||[name isEqual:@"VirusControl/SelectedUtilityPage"]||[name isEqual:@"Multi Parameters/Panel Part"])return YES;
 ParamID id=[self paramID:name part:[self partOf:node]];auto* p=id==kNoParamId?nullptr:controller->getParameterObject(id);
 if(p)return !(p->getInfo().flags&ParameterInfo::kIsReadOnly)&&editableBindings.count(id)!=0;
 NSDictionary* definition=[self definition:name];
 return NO;
}
- (BOOL)point:(NSPoint)point insideNode:(NSDictionary*)node {
 if(!NSPointInRect(point,rectOf(node[@"absolute_rect"])))return NO;
 id ancestor=node[@"parent_id"];
 while(ancestor&&ancestor!=[NSNull null]){NSDictionary* parent=nodeByID[ancestor];if(!parent)break;if(!NSPointInRect(point,rectOf(parent[@"absolute_rect"])))return NO;ancestor=parent[@"parent_id"];}
 return YES;
}
- (NSDictionary*)nodeAt:(NSPoint)point {
 // Original masked knob artwork surrounds graph handles but is later in XML
 // draw order. Prefer an actual graph handle over its enclosing knob rectangle.
 for(NSDictionary* node in [nodes reverseObjectEnumerator])if(Graphs::supports(node[@"type"])&&[self visible:node]&&[self editable:node]&&[self point:point insideNode:node]){
  NSInteger part=[self partOf:node];
  if(Graphs::hit(node[@"type"],node[@"attrs"],rectOf(node[@"absolute_rect"]),^double(NSString* name){return [self plain:name part:part];},point)>=0)return node;
 }
 for(NSDictionary* node in [nodes reverseObjectEnumerator])if([self visible:node]&&[self editable:node]&&[self point:point insideNode:node])return node;
 return nil;
}
- (void)applyNode:(NSDictionary*)node plain:(double)value gesture:(BOOL)gesture {
 NSString* name=node[@"attrs"][@"Parameter"];
 if([name isEqual:@"VirusControl/SelectedPage"])selectedPage=static_cast<NSInteger>(value);
 else if([name isEqual:@"Multi Parameters/Panel Part"])selectedPart=std::clamp(static_cast<NSInteger>(value),0L,15L);
 else if([name isEqual:@"VirusControl/SelectedUtilityPage"])selectedUtility=static_cast<NSInteger>(value);
 else if([name isEqual:@"VirusControl/Selected Section"])selectedSection=static_cast<NSInteger>(value);
 else {if(![self editable:node])return;[self writeParameter:name part:[self partOf:node] value:value gesture:gesture];}
 [self setNeedsDisplay:YES];
 [self updateBrowserVisibility];
}
- (void)updateBrowserVisibility {
 BOOL hide=selectedSection!=0||selectedPage!=1;
 if(hide&&!browser.hidden)[self.window makeFirstResponder:nil];
 browser.hidden=hide;
}
- (void)recallPatch:(NSDictionary*)patch {
 NSData* data=patch[@"sysex"];
 if(![data isKindOfClass:NSData.class])return;
 auto* bytes=static_cast<const uint8_t*>(data.bytes);controller->hardwarePreset(std::vector<uint8_t>(bytes,bytes+data.length),unsigned(selectedPart));
 if(!controller->hardwareConnected){suppressPresetAutomation=YES;[self loadOfflinePatch:patch];suppressPresetAutomation=NO;}
}
- (void)synchronizeHardwarePatch {
 if(displayedPatchRevision==controller->patchRevision)return;
 auto previousPart=selectedPart;suppressPresetAutomation=YES;
 for(unsigned part=0;part<16;++part)try{auto patch=virus::patches::parse_single(controller->hardwarePatches.parts[part]);NSMutableDictionary* values=[NSMutableDictionary dictionary];for(auto& value:patch.decoded_values())values[@(value.name)]=@(value.value);
  selectedPart=part;[self loadOfflinePatch:@{@"name":@(patch.name.c_str()),@"verifiedValues":values}];
 }catch(const std::exception&){}
 suppressPresetAutomation=NO;selectedPart=previousPart;displayedPatchRevision=controller->patchRevision;
 NSMutableArray* buffers=[NSMutableArray array];for(auto& bytes:controller->hardwarePatches.parts)[buffers addObject:[NSData dataWithBytes:bytes.data() length:bytes.size()]];[browser setHardwareBuffers:buffers];

}
- (void)loadOfflinePatch:(NSDictionary*)patch {
 NSDictionary* values=patch[@"verifiedValues"];
 if(![values isKindOfClass:NSDictionary.class]||!values.count)return;
 [self finishGraphGesture];
 // A partial legacy dump must not leave extended values from a previous patch.
 // Unknown/unmapped fields use XML defaults; this is explicitly partial recall.
 NSString* prefix=[NSString stringWithFormat:@"%ld:",(long)selectedPart];
 for(NSString* key in localValues.allKeys)if([key hasPrefix:prefix])[localValues removeObjectForKey:key];
 for(const auto& spec:kCatalog)if(spec.part==selectedPart+1){NSString* parameter=@(spec.name);NSDictionary* definition=[self definition:parameter];if(definition)[self writeParameter:parameter part:selectedPart value:[definition[@"defaultValue"] doubleValue] gesture:YES];}
 for(NSString* name in values)if([values[name] isKindOfClass:NSNumber.class])[self writeParameter:name part:selectedPart value:[values[name] doubleValue] gesture:YES];
 NSString* name=patch[@"name"];if([name isKindOfClass:NSString.class])patchNames[@(selectedPart)]=name;
 [self setNeedsDisplay:YES];
}
- (void)finishGraphGesture {
 for(NSNumber* identifier in graphEditIDs)controller->endEdit(identifier.unsignedIntValue);
 [graphEditIDs removeAllObjects];graphDragging=nil;graphHandle=-1;
}
- (void)writeParameter:(NSString*)name part:(NSInteger)part value:(double)value gesture:(BOOL)gesture {
 if(!name.length||!std::isfinite(value)||part<0||part>15)return;
 NSDictionary* definition=[self definition:name];
 if(!definition)return;
 double minimum=[definition[@"minValue"] doubleValue],maximum=[definition[@"maxValue"] doubleValue];
 if(maximum<minimum)return;
 value=std::round(std::clamp(value,minimum,maximum));
 ParamID id=[self paramID:name part:part];
 if(id==kNoParamId){
  if(![definition[@"part"] isEqual:@"all"]||[name hasPrefix:@"Internal "]||[name hasPrefix:@"VirusControl/"]||[name hasPrefix:@"LockView/"]||[name hasPrefix:@"Soft Knobs/"])return;
  localValues[bindingKey(name,part)]=@(value);
 }else{
  auto* parameter=controller->getParameterObject(id);if(!parameter||(parameter->getInfo().flags&ParameterInfo::kIsReadOnly))return;
  double normalized=std::clamp(controller->plainParamToNormalized(id,value),0.,1.);
  if(gesture&&!suppressPresetAutomation)controller->beginEdit(id);
  controller->setParamNormalized(id,normalized);if(!suppressPresetAutomation)controller->performEdit(id,normalized);
  if(gesture&&!suppressPresetAutomation)controller->endEdit(id);
 }
 [self setNeedsDisplay:YES];
}
- (void)selectMenu:(NSMenuItem*)sender {[self applyNode:sender.representedObject plain:sender.tag gesture:YES];}
- (void)mouseDown:(NSEvent*)event {
 NSPoint point=[self convertPoint:event.locationInWindow fromView:nil];
 NSDictionary* node=[self nodeAt:point];if(!node)return;focused=node;[self.window makeFirstResponder:self];
 if(Graphs::supports(node[@"type"])){
  [self finishGraphGesture];graphPart=[self partOf:node];
  Graphs::Read read=^double(NSString* name){return [self plain:name part:graphPart];};
  graphHandle=Graphs::hit(node[@"type"],node[@"attrs"],rectOf(node[@"absolute_rect"]),read,point);
  if(graphHandle<0)return;
  graphDragging=node;
  for(NSString* name in Graphs::editingParameters(node[@"type"],node[@"attrs"],rectOf(node[@"absolute_rect"]),read,graphHandle)){
   ParamID id=[self paramID:name part:graphPart];auto* parameter=id==kNoParamId?nullptr:controller->getParameterObject(id);
   if(parameter&&!(parameter->getInfo().flags&ParameterInfo::kIsReadOnly)&&![graphEditIDs containsObject:@(id)]){[graphEditIDs addObject:@(id)];controller->beginEdit(id);}
  }
  return;
 }
 NSDictionary* a=node[@"attrs"];NSString* type=node[@"type"];NSString* name=a[@"Parameter"];NSInteger part=[self partOf:node];double current=[self plain:name part:part];NSDictionary* definition=[self definition:name];double minimum=[definition[@"minValue"] doubleValue],maximum=[definition[@"maxValue"] doubleValue];
 if((event.modifierFlags&NSEventModifierFlagCommand)&&![name hasPrefix:@"VirusControl/"]&&![name isEqual:@"Multi Parameters/Panel Part"]){[self applyNode:node plain:[definition[@"defaultValue"] doubleValue] gesture:YES];return;}
 if([type isEqual:@"RadioButton"]){[self applyNode:node plain:[a[@"OnValue"] doubleValue] gesture:YES];return;}
 if([type isEqual:@"Button"]){[self applyNode:node plain:current==minimum?maximum:minimum gesture:YES];return;}
 if([type isEqual:@"IncButton"]||[type isEqual:@"DecButton"]){[self applyNode:node plain:std::clamp(current+([type isEqual:@"IncButton"]?1:-1),minimum,maximum) gesture:YES];return;}
 if([type containsString:@"Menu"]){
  NSMenu* menu=[[NSMenu alloc]initWithTitle:name];NSArray* entries=model[@"valuelists"][definition[@"valuelist"]][@"entries"];
  if(entries.count){for(NSDictionary* entry in entries){NSDictionary* a=entry[@"attrs"];NSInteger code=[(a[@"value"]?:a[@"index"]) integerValue];if(code<minimum||code>maximum)continue;NSString* title=entry[@"text"]?:[NSString stringWithFormat:@"%ld",(long)code];NSMenuItem* item=[[NSMenuItem alloc]initWithTitle:title action:@selector(selectMenu:) keyEquivalent:@""];item.target=self;item.tag=code;item.representedObject=node;item.state=current==code?NSControlStateValueOn:NSControlStateValueOff;[menu addItem:item];}}
  else for(int i=minimum;i<=maximum&&i<minimum+256;++i){NSMenuItem* item=[[NSMenuItem alloc]initWithTitle:[self textFor:node value:i] action:@selector(selectMenu:) keyEquivalent:@""];item.target=self;item.tag=i;item.representedObject=node;[menu addItem:item];}
  [NSMenu popUpContextMenu:menu withEvent:event forView:self];return;
 }
 if(event.clickCount==2||(event.modifierFlags&NSEventModifierFlagCommand)){[self applyNode:node plain:[definition[@"defaultValue"] doubleValue] gesture:YES];return;}
 dragging=node;dragStart=event.locationInWindow;dragValue=current;dragID=[self paramID:name part:part];if(dragID!=kNoParamId)controller->beginEdit(dragID);
}
- (void)mouseDragged:(NSEvent*)event {
 if(graphDragging){
  Graphs::Read read=^double(NSString* name){return [self plain:name part:graphPart];};
  for(NSDictionary* edit in Graphs::edits(graphDragging[@"type"],graphDragging[@"attrs"],rectOf(graphDragging[@"absolute_rect"]),read,graphHandle,[self convertPoint:event.locationInWindow fromView:nil]))
   [self writeParameter:edit[@"parameter"] part:graphPart value:[edit[@"value"] doubleValue] gesture:NO];
  return;
 }
 if(!dragging)return;NSDictionary* definition=[self definition:dragging[@"attrs"][@"Parameter"]];double lo=[definition[@"minValue"] doubleValue],hi=[definition[@"maxValue"] doubleValue];double delta=[dragging[@"type"]isEqual:@"VUSlider"]?(event.locationInWindow.x-dragStart.x)*180./(rectOf(dragging[@"absolute_rect"]).size.width-(originalUI?21:8)):(event.locationInWindow.y-dragStart.y)+(event.locationInWindow.x-dragStart.x)*.3;delta/=self.frame.size.width/907.;double scale=(event.modifierFlags&NSEventModifierFlagShift)?1000:180;[self applyNode:dragging plain:std::round(std::clamp(dragValue+delta*(hi-lo)/scale,lo,hi)) gesture:NO];
}
- (void)mouseUp:(NSEvent*)event {[self finishGraphGesture];if(dragID!=kNoParamId)controller->endEdit(dragID);dragging=nil;dragID=kNoParamId;}
- (void)mouseMoved:(NSEvent*)event {
 NSPoint point=[self convertPoint:event.locationInWindow fromView:nil];NSDictionary* previous=hovered;hovered=nil;
 for(NSDictionary* node in [nodes reverseObjectEnumerator])if([self visible:node]&&NSPointInRect(point,rectOf(node[@"absolute_rect"]))&&node[@"attrs"][@"Parameter"]){hovered=node;break;}
 if(hovered==previous)return;
 self.toolTip=hovered?([hovered[@"type"]isEqual:@"VUSlider"]?@"MIDI note activity with CC7 volume handle. Not an audio dB meter.":[self editable:hovered]?hovered[@"attrs"][@"Parameter"]:[hovered[@"attrs"][@"Parameter"] stringByAppendingString:@" — hardware editing unavailable in this beta"]):nil;[self setNeedsDisplayInRect:NSMakeRect(211,470,431,12)];
}
- (void)mouseExited:(NSEvent*)event {if(hovered){hovered=nil;self.toolTip=nil;[self setNeedsDisplayInRect:NSMakeRect(211,470,431,12)];}}
- (void)keyDown:(NSEvent*)event {
 if(event.keyCode==53){focused=nil;[self setNeedsDisplay:YES];return;}
 if(event.keyCode==48){NSMutableArray* controls=[NSMutableArray array];for(NSDictionary* node in nodes)if([self visible:node]&&[self editable:node]&&!Graphs::supports(node[@"type"])&&![node[@"attrs"][@"Parameter"]hasPrefix:@"Repository Actions/"])[controls addObject:node];
  if(controls.count){NSInteger index=focused?[controls indexOfObject:focused]:NSNotFound;BOOL reverse=event.modifierFlags&NSEventModifierFlagShift;index=index==NSNotFound?(reverse?controls.count-1:0):(index+(reverse?-1:1)+controls.count)%controls.count;focused=controls[index];hovered=focused;[self setNeedsDisplay:YES];}return;
 }
 if(originalUI)Skin::box(NSMakeRect(0,470,907,43),Skin::Palette(true).base,nil,0);
 if(focused&&[self visible:focused]){
  NSString* type=focused[@"type"];NSDictionary* a=focused[@"attrs"];
  if(event.keyCode==49||event.keyCode==36){NSRect r=rectOf(focused[@"absolute_rect"]);NSPoint point=[self convertPoint:NSMakePoint(NSMidX(r),NSMidY(r)) toView:nil];NSEvent* click=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:point modifierFlags:0 timestamp:event.timestamp windowNumber:self.window.windowNumber context:nil eventNumber:0 clickCount:1 pressure:1];[self mouseDown:click];[self mouseUp:click];return;}
  if(event.keyCode>=123&&event.keyCode<=126){double value=[self plain:a[@"Parameter"] part:[self partOf:focused]];bool forward=event.keyCode==124||event.keyCode==126;
   if([type isEqual:@"RadioButton"]){std::vector<double> choices;for(NSDictionary* node in nodes)if([node[@"type"]isEqual:@"RadioButton"]&&[node[@"attrs"][@"Parameter"]isEqual:a[@"Parameter"]]&&[self visible:node])choices.push_back([node[@"attrs"][@"OnValue"]doubleValue]);std::sort(choices.begin(),choices.end());choices.erase(std::unique(choices.begin(),choices.end()),choices.end());if(!choices.empty()){auto it=std::lower_bound(choices.begin(),choices.end(),value);int i=int(it-choices.begin());i=(i+(forward?1:-1)+choices.size())%choices.size();[self applyNode:focused plain:choices[i] gesture:YES];}return;}
   [self applyNode:focused plain:value+(forward?1:-1) gesture:YES];return;}
 }
 [super keyDown:event];
}
- (void)scrollWheel:(NSEvent*)event {if(event.scrollingDeltaY==0)return;NSDictionary* node=[self nodeAt:[self convertPoint:event.locationInWindow fromView:nil]];if(!node||Graphs::supports(node[@"type"]))return;NSDictionary* a=node[@"attrs"];if([a[@"Parameter"] hasPrefix:@"VirusControl/"])return;double v=[self plain:a[@"Parameter"] part:[self partOf:node]];[self applyNode:node plain:v+(event.scrollingDeltaY>0?1:-1) gesture:YES];}
@end

namespace VirusRework {
class NativeEditor final:public CPluginView {
 IPtr<Controller> controller_;
 __strong VTRNativeEditor* view_=nil;
public:
 explicit NativeEditor(Controller* c):controller_(c){NSInteger zoom=[[[NSUserDefaults alloc]initWithSuiteName:@"org.virus-ti-rework.editor"]integerForKey:@"scalePercent"];double scale=zoom>=100&&zoom<=200?zoom/100.:1.25;rect={0,0,int32(std::lround(907*scale)),int32(std::lround(513*scale))};}
 ~NativeEditor() override {removed();}
 tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override {return type&&std::strcmp(type,kPlatformTypeNSView)==0?kResultTrue:kResultFalse;}
 tresult PLUGIN_API attached(void* parent,FIDString type) override {
  if(!parent||isPlatformTypeSupported(type)!=kResultTrue||view_)return kResultFalse;
  NSString* resources=[[[NSBundle bundleForClass:VTRNativeEditor.class]resourcePath]stringByAppendingPathComponent:@"OriginalEditor"];
  view_=[[VTRNativeEditor alloc]initWithController:controller_.get() resources:resources];
  if(!view_->model){view_=nil;return kResultFalse;}
  [view_ setEditorSize:NSMakeSize(rect.getWidth(),rect.getHeight())];
  view_->resizeHandler=^(double scale){ViewRect desired{0,0,int32(std::lround(907*scale)),int32(std::lround(513*scale))};if(plugFrame)plugFrame->resizeView(this,&desired);};
  [(__bridge NSView*)parent addSubview:view_];systemWindow=parent;return kResultTrue;
 }
 tresult PLUGIN_API removed() override {if(view_){view_->resizeHandler=nil;[view_ removeFromSuperview];view_=nil;}systemWindow=nullptr;return kResultTrue;}
 tresult PLUGIN_API canResize() override {return kResultTrue;}
 tresult PLUGIN_API checkSizeConstraint(ViewRect* r) override {if(!r)return kInvalidArgument;auto width=std::clamp(r->getWidth(),907,1814);r->right=r->left+width;r->bottom=r->top+int32(std::lround(width*513./907.));return kResultTrue;}
 tresult PLUGIN_API onSize(ViewRect* r) override {
  if(!r||r->getWidth()<907||r->getWidth()>1814||std::abs(r->getHeight()-std::lround(r->getWidth()*513./907.))>1)return kResultFalse;
  if(view_)[view_ setEditorSize:NSMakeSize(r->getWidth(),r->getHeight())];
  [[[NSUserDefaults alloc]initWithSuiteName:@"org.virus-ti-rework.editor"]setInteger:std::lround(r->getWidth()*100./907) forKey:@"scalePercent"];
  return CPluginView::onSize(r);
 }
};
IPlugView* createNativeEditor(Controller* c){return new NativeEditor(c);}
}
