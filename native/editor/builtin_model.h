#pragma once
#import <Foundation/Foundation.h>
#include "../vst3/parameter_registry.h"
#include <cstring>
namespace VirusRework {
inline NSDictionary* builtinEditorModel() {
 NSMutableDictionary* parameters=[NSMutableDictionary dictionary];NSMutableArray* nodes=[NSMutableArray array];
 auto parameter=[&](NSString* name,double lo,double hi,double initial){parameters[name]=@{@"attrs":@{@"minValue":@(lo),@"maxValue":@(hi),@"defaultValue":@(initial),@"part":@"all"}};};
 auto node=[&](NSString* type,NSString* name,int part,int page,double x,double y,double w,double h,NSNumber* on=nil){
  NSMutableDictionary* a=[@{@"Parameter":name,@"Part":@(part)} mutableCopy];if(on)a[@"OnValue"]=on;
  [nodes addObject:@{@"id":[NSString stringWithFormat:@"owned%lu",nodes.count],@"type":type,@"attrs":a,@"page":@(page),@"parent_id":NSNull.null,@"absolute_rect":@{@"left":@(x),@"top":@(y),@"width":@(w),@"height":@(h)},@"sprites":@{}}];
 };
 parameter(@"VirusControl/SelectedPage",0,12,0);parameter(@"Multi Parameters/Panel Part",0,15,0);
 const int pages[]={0,1,2,3,4,5,6,7,11,9,8,12};
 for(int i=0;i<12;++i)node(@"RadioButton",@"VirusControl/SelectedPage",-1,-1,203+i*57,15,56,25,@(pages[i]));
 for(int p=0;p<16;++p){node(@"RadioButton",@"Multi Parameters/Panel Part",p,-1,12,52+p*26,25,23,@(p));node(@"NameEdit",@"Patch Name",p,-1,42,56+p*26,112,18);node(@"VUSlider",@"Internal MIDI Standard Parameters/07 Volume",p,-1,158,58+p*26,27,12);}
 int slots[13]{};
 auto pageFor=[](NSString* name){if([name hasPrefix:@"Osc"]||[name hasPrefix:@"Noise"]||[name hasPrefix:@"Subosc"]||[name hasPrefix:@"Ring"]||[name hasPrefix:@"Unison"])return 2;
  if([name hasPrefix:@"Filter "]&&![name hasPrefix:@"Filter Bank"])return 3;if([name containsString:@"Envelope"])return 3;
  if([name hasPrefix:@"LFO"])return 4;if([name hasPrefix:@"Matrix"])return 5;if([name hasPrefix:@"Arpeggiator"])return 6;
  if([name hasPrefix:@"Delay"]||[name hasPrefix:@"Reverb"]||[name hasPrefix:@"Vocoder"])return 11;
  if([name hasPrefix:@"Chorus"]||[name hasPrefix:@"Phaser"]||[name hasPrefix:@"Patch Distortion"]||[name hasPrefix:@"Analog Boost"]||[name hasPrefix:@"Filter Bank"])return 7;return 9;};
 auto control=[&](NSString* name,double lo,double hi,double initial,int page){parameter(name,lo,hi,initial);int i=slots[page]++;double x=213+(i%8)*83,y=85+(i/8)*58;node(hi-lo<24?@"OptionMenu":@"Knob",name,-1,page,x+17,y,hi-lo<24?61:35,hi-lo<24?24:35);};
 for(const auto& c:kControls){NSString* name=@(c.name);parameter(name,c.minimum,c.maximum,c.initial);if([name hasPrefix:@"Internal "])continue;control(name,c.minimum,c.maximum,c.initial,pageFor(name));}
 // Explicit disabled roadmap controls keep pages discoverable without pretending to send hardware edits.
 for(int slot=1;slot<=6;++slot)for(NSString* field in @[@"Source",@"Destination 1",@"Amount 1",@"Destination 2",@"Amount 2",@"Destination 3",@"Amount 3"])
  control([NSString stringWithFormat:@"Matrix %d/%@",slot,field],0,[field hasPrefix:@"Amount"]?127:15,0,5);
 for(NSString* field in @[@"Mode",@"Pattern",@"Clock",@"Octaves"])control([@"Arpeggiator/" stringByAppendingString:field],0,15,0,6);
 for(int osc=1;osc<=2;++osc)control([NSString stringWithFormat:@"Oscillator %d/Mode",osc],0,7,0,2);
 parameter(@"Oscillator 1/PulseWidth",0,127,64);parameter(@"Oscillator 2/PulseWidth",0,127,64);
 node(@"OscClassicShape",@"Oscillator 1/PulseWidth",-1,2,230,370,290,66);node(@"OscClassicShape",@"Oscillator 2/PulseWidth",-1,2,560,370,290,66);
 node(@"LP4Response",@"Filter 1/Cutoff",-1,3,230,350,290,86);node(@"ADSTREnvelope",@"Amp Envelope/Attack",-1,3,560,350,290,86);
 return @{@"project_owned":@YES,@"nodes":nodes,@"parameters":parameters,@"conditions":@{},@"valuelists":@{}};
}
}
