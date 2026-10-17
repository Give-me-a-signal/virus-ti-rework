#pragma once
#import <Foundation/Foundation.h>
#include "../vst3/parameter_registry.h"

namespace VirusRework {
// Project-authored vector layout, in the editor's established 907 x 513 space.
// Explicit page/group placement is independent of registry/automation ordering.
// No extracted XML, bitmaps, factory content or imported-resource lookup.
inline NSDictionary* builtinEditorModel() {
 NSMutableDictionary* parameters=[NSMutableDictionary dictionary];
 NSMutableDictionary* lists=[NSMutableDictionary dictionary];
 NSMutableDictionary* conditions=[NSMutableDictionary dictionary];
 NSMutableArray* nodes=[NSMutableArray array];
 auto parameter=[&](NSString* name,double lo=0,double hi=127,double initial=0){
  if(!parameters[name])parameters[name]=@{@"attrs":@{@"minValue":@(lo),@"maxValue":@(hi),@"defaultValue":@(initial),@"part":@"all"}};
 };
 for(const auto& c:kControls)parameter(@(c.name),c.minimum,c.maximum,c.initial);
 auto choices=[&](NSString* name,NSArray<NSString*>* labels,int initial=0){
  parameter(name,0,labels.count-1,initial);NSMutableDictionary* d=[parameters[name][@"attrs"] mutableCopy];d[@"valuelist"]=name;parameters[name]=@{@"attrs":d};
  NSMutableArray* values=[NSMutableArray array];for(NSUInteger i=0;i<labels.count;++i)[values addObject:@{@"attrs":@{@"value":@(i)},@"text":labels[i]}];lists[name]=values;
 };
 auto node=[&](NSString* type,NSString* name,int page,double x,double y,double w,double h,NSDictionary* extra=nil,int part=-1){
  parameter(name);NSMutableDictionary* a=[@{@"Parameter":name,@"Part":@(part)} mutableCopy];if(extra)[a addEntriesFromDictionary:extra];
  [nodes addObject:@{@"id":[NSString stringWithFormat:@"layout-%lu",nodes.count],@"type":type,@"attrs":a,@"page":@(page),@"parent_id":NSNull.null,@"absolute_rect":@{@"left":@(x),@"top":@(y),@"width":@(w),@"height":@(h)},@"sprites":@{}}];
 };
 auto knob=[&](NSString* name,int page,double cx,double cy,double side=40,NSString* label=nil){NSString* caption=label?:name.lastPathComponent;if(!label&&([name isEqual:@"Delay/Send"]||[name isEqual:@"Reverb/Send"]))caption=[name stringByReplacingOccurrencesOfString:@"/Send" withString:@" send"];node(@"Knob",name,page,cx-side/2,cy-side/2,side,side,@{@"Label":caption});};
 auto ring=[&](NSString* name,int page,double cx,double cy,double side,NSString* label){node(@"MaskedKnob",name,page,cx-side/2,cy-side/2,side,side,@{@"Label":label,@"ShowLabel":@YES});};
 auto menu=[&](NSString* name,int page,double x,double y,double w=110,NSString* label=nil){node(@"OptionMenu",name,page,x,y,w,22,@{@"Label":label?:@""});};
 auto slider=[&](NSString* name,int page,double x,double y,double w,NSString* label){node(@"HorizontalSlider",name,page,x,y,w,14,@{@"Label":label});};
 auto button=[&](NSString* name,int page,double x,double y,double w,NSString* text){node(@"Button",name,page,x,y,w,24,@{@"Label":text});};
 auto radio=[&](NSString* name,int page,double x,double y,double w,double h,int value,NSString* text){node(@"RadioButton",name,page,x,y,w,h,@{@"OnValue":@(value),@"Label":text});};
 auto graph=[&](NSString* type,NSString* name,int page,double x,double y,double w,double h){node(type,name,page,x,y,w,h);};
 auto note=[&](NSString* text,int page,double x,double y,double w,double h=22){node(@"StaticText",@"",page,x,y,w,h,@{@"Text":text});};
 auto when=[&](NSString* name,int lo,int hi){NSString* key=[NSString stringWithFormat:@"layout:%@:%d:%d",name,lo,hi];conditions[key]=@{@"ast":@{@"op":@"test",@"type":@"inside",@"parameter":name,@"min":@(lo),@"max":@(hi)}};nodes.lastObject[@"attrs"][@"Condition"]=key;};
 auto row=[&](NSArray<NSString*>* names,int page,double x,double y,double step,double side=37){for(NSUInteger i=0;i<names.count;++i)knob(names[i],page,x+i*step,y,side);};
 parameter(@"VirusControl/SelectedPage",0,12,0);parameter(@"Multi Parameters/Panel Part",0,15,0);
 parameter(@"VirusControl/Selected Section",0,1,0);parameter(@"VirusControl/SelectedUtilityPage",0,1,0);
 const int pages[]={0,1,2,3,4,5,6,7,11,9};
 for(int i=0;i<10;++i){radio(@"VirusControl/SelectedPage",-1,213+i*62.5,15,60,25,pages[i],@"");when(@"VirusControl/Selected Section",0,0);}
 node(@"Button",@"VirusControl/Selected Section",-1,839,15,54,25);
 for(int i=0;i<2;++i){radio(@"VirusControl/SelectedUtilityPage",-1,213+i*90,15,85,25,i,@"");when(@"VirusControl/Selected Section",1,1);}
 for(int p=0;p<16;++p){
  radio(@"Multi Parameters/Panel Part",-1,12,52+p*26,25,23,p,@"");
  node(@"Button",@"Multi Part Parameter/Mute",-1,36,53+p*26,15,9,@{@"Label":@"M"},p);
  node(@"Button",@"Multi Part Parameter/Direct",-1,36,64+p*26,15,9,@{@"Label":@"D"},p);
  node(@"NameEdit",@"Patch Name",-1,55,53+p*26,103,10,nil,p);
  node(@"VUSlider",@"Internal MIDI Standard Parameters/07 Volume",-1,55,63+p*26,103,12,nil,p);
  node(@"Knob",@"Multi Part Parameter/Volume",-1,164,54+p*26,20,20,@{@"Label":@""},p);
 }
 // Easy: the large central cutoff and the lower soft-knob/effect groups.
 ring(@"Oscillators/Volume+Saturation",0,273,184,82,@"Osc vol / sat");
 ring(@"Suboscillator/Volume",0,379,184,82,@"Sub osc");ring(@"Ring Modulator/Volume",0,485,184,82,@"Ring mod");
 ring(@"Filter 1/Cutoff",0,625,184,124,@"Cutoff");graph(@"LP4Response",@"Filter 1/Cutoff",0,585,153,80,60);
 ring(@"Filter 1/Resonance",0,766,184,82,@"Resonance");
 knob(@"Filter Envelope/Attack",0,846,157,40,@"Attack");knob(@"Filter Envelope/Decay",0,846,210,40,@"Decay");
 for(int i=0;i<3;++i){knob([NSString stringWithFormat:@"Softknobs/Softknob %d",i+1],0,246+i*133,397,40);menu([NSString stringWithFormat:@"Soft Knobs/Name %d",i+1],0,278+i*133,387,59);}
 row(@[@"Delay/Send",@"Reverb/Send",@"Chorus/Mix"],0,664,397,91,40);
 // Oscillators: left/right large shape controls, common column and lower mixer.
 parameter(@"Oscillator 1/PulseWidth",0,127,64);parameter(@"Oscillator 2/PulseWidth",0,127,64);
 for(int osc=1;osc<=2;++osc){double dx=osc==1?0:422;NSString* group=[NSString stringWithFormat:@"Oscillator %d",osc];
  NSString* mode=[group stringByAppendingString:@"/Mode"];
  choices(mode,@[@"Classic",@"HyperSaw",@"Wavetable",@"Wave PWM",@"Grain",@"Grain complex",@"Formant",@"Formant complex"]);
  menu([group stringByAppendingString:@"/Mode"],2,269+dx,70,133);
  ring([group stringByAppendingString:@"/Shape"],2,337+dx,164,124,@"Shape");when(mode,0,0);
  graph(@"OscClassicShape",[group stringByAppendingString:@"/PulseWidth"],2,301+dx,149,75,32);when(mode,0,0);
  NSString* hyper=[group stringByAppendingString:@" HyperSaw"],*wave=[group stringByAppendingString:@" Wavetable"];
  ring([hyper stringByAppendingString:@"/Density"],2,337+dx,164,124,@"Density");when(mode,1,1);
  graph(@"OscHypersawShape",[hyper stringByAppendingString:@"/DetuneSpread"],2,301+dx,149,75,32);when(mode,1,1);
  ring([wave stringByAppendingString:@"/WaveTableIndex"],2,337+dx,164,124,@"Wave index");when(mode,2,7);
  node(@"HorizontalKnob",[group stringByAppendingString:@"/WaveSelect"],2,301+dx,149,75,32);when(mode,2,7);
  knob([group stringByAppendingString:@"/Semitone"],2,osc==1?426:667,119,40);
  knob(osc==1?@"Velocity Response/Osc1 Shape":@"Velocity Response/Osc2 Shape",2,osc==1?426:667,208,40,osc==1?@"Vel → osc 1":@"Vel → osc 2");
  knob([group stringByAppendingString:@"/Key Follow"],2,osc==1?246:846,208,40,@"Key follow");
  if(osc==1){knob(@"Oscillator 1/Balance",2,246,119,40,@"Wave balance");}
  else {knob(@"Oscillator 2/Detune",2,846,119,40,@"Detune");}
  knob([wave stringByAppendingString:@"/Interpolation"],2,337+dx,260,40,@"Interpolation");when(mode,2,7);
  knob([hyper stringByAppendingString:@"/DetuneSpread"],2,337+dx,260,40,@"Detune spread");when(mode,1,1);
  menu([group stringByAppendingString:@"/PulseWidth"],2,280+dx,263,116);when(mode,0,0);
 }
 button(@"Oscillators/Sync",2,483,76,29,@"Sync");knob(@"Oscillators/FilterEnv > Pitch",2,597,88,40,@"Env → pitch");
 knob(@"Oscillators/Phase Init",2,496,138,40,@"Phase init");knob(@"Velocity Response/Pulse Width",2,547,138,40,@"Vel → PW");knob(@"Patch Common/Portamento",2,597,138,40);
 knob(@"Oscillator 2/FM Amount",2,496,208,40,@"FM amount");knob(@"Oscillators/FilterEnv > FM",2,547,208,40,@"Env → FM");knob(@"Velocity Response/FM Amount",2,597,208,40,@"Vel → FM");
 choices(@"Oscillators/FM Mode",@[@"Pos Triangle",@"Triangle",@"Wave",@"Noise",@"Input"]);menu(@"Oscillators/FM Mode",2,480,249,130);
 choices(@"Oscillator 3/Mode",@[@"Off",@"Slave",@"Saw",@"Square",@"Triangle",@"Sine"]);menu(@"Oscillator 3/Mode",2,221,333,107);
 knob(@"Oscillator 3/Semitone",2,375,344,40);knob(@"Oscillator 3/Detune",2,426,344,40);
 choices(@"Unison/Mode",@[@"Off",@"2 voices",@"3 voices",@"4 voices",@"5 voices",@"6 voices",@"7 voices",@"8 voices"]);menu(@"Unison/Mode",2,221,417,80);
 row(@[@"Unison/Detune",@"Unison/Pan Spread",@"Unison/LFO Phase"],2,326,430,50,40);
 knob(@"Noise Oscillator/Color",2,496,344,38,@"Color");knob(@"Patch Common/Punch Intensity",2,496,430,38,@"Intensity");
 slider(@"Oscillator 1/Balance",2,547,334,125,@"Balance");slider(@"Oscillators/Volume+Saturation",2,717,334,125,@"Osc vol / sat");
 slider(@"Oscillator 3/Volume",2,547,378,125,@"Osc 3");slider(@"Noise Oscillator/Volume",2,717,378,125,@"Noise");
 slider(@"Suboscillator/Volume",2,547,422,125,@"Sub osc");slider(@"Ring Modulator/Volume",2,717,422,125,@"Ring mod");
 // Filters and envelopes. Large cutoff rings share centres with their displays.
 for(int f=1;f<=2;++f){double left=f==1?211:634;NSString* group=[NSString stringWithFormat:@"Filter %d",f];
  choices([group stringByAppendingString:@"/Mode"],@[@"LP",@"HP",@"BP",@"BS",@"AN 1",@"AN 2",@"AN 3",@"AN 4"]);
  for(int mode=0;mode<4;++mode)radio([group stringByAppendingString:@"/Mode"],3,left+62+mode*32,74,30,14,mode,@[@"LP",@"HP",@"BP",@"BS"][mode]);
  ring([group stringByAppendingString:@"/Cutoff"],3,left+126,163,124,@"Cutoff");
  for(int response=0;response<4;++response){graph(@[@"LP4Response",@"HP4Response",@"BP4Response",@"BS4Response"][response],[group stringByAppendingString:@"/Cutoff"],3,left+85,133,82,60);when([group stringByAppendingString:@"/Mode"],response,response==0?0:response);}
  graph(@"LP4Response",[group stringByAppendingString:@"/Cutoff"],3,left+85,133,82,60);when([group stringByAppendingString:@"/Mode"],4,7);
  knob([group stringByAppendingString:@"/Envelope Amount"],3,left+(f==1?35:213),119,40,@"Env amount");
  knob([group stringByAppendingString:@"/Resonance"],3,left+(f==1?213:35),119,40,@"Resonance");
  knob([NSString stringWithFormat:@"Velocity Response/Filter %d Envelope Amount",f],3,left+(f==1?35:213),208,40,@"Vel → env");
  knob([NSString stringWithFormat:@"Velocity Response/Filter %d Resonance",f],3,left+(f==1?213:35),208,40,@"Vel → reso");
  knob([group stringByAppendingString:@"/Key Follow"],3,left+107,262,38,@"Key follow");
 }
 slider(@"Oscillators/Volume+Saturation",3,483,76,129,@"Osc vol / sat");choices(@"Filters/Routing",@[@"SER 4",@"SER 6",@"PAR",@"SPLIT"]);
 for(int n=0;n<4;++n)radio(@"Filters/Routing",3,483+n*32,110,30,14,n,@[@"SER 4",@"SER 6",@"PAR",@"SPLIT"][n]);
 choices(@"Filters/Saturation",@[@"Off",@"Soft",@"Hard",@"Digital"]);menu(@"Filters/Saturation",3,483,176,129);
 choices(@"Filters/Keyfollow Base",@[@"Keyfollow Base C2"]);menu(@"Filters/Keyfollow Base",3,483,208,129);
 button(@"Filters/Cutoff Link",3,483,237,62,@"Cutoff link");button(@"Filters/Filter Link",3,548,237,64,@"Filter link");slider(@"Filters/Balance",3,483,269,129,@"Balance");
 graph(@"ADSTREnvelope",@"Filter Envelope/Attack",3,240,326,209,78);graph(@"ADSTREnvelope",@"Amp Envelope/Attack",3,646,326,222,78);
 NSArray* stages=@[@"Attack",@"Decay",@"Sustain",@"Sustain Time",@"Release"];
 for(NSUInteger i=0;i<stages.count;++i){knob([@"Filter Envelope/" stringByAppendingString:stages[i]],3,236+i*50,433,37);knob([@"Amp Envelope/" stringByAppendingString:stages[i]],3,650+i*50,433,37);}
 knob(@"Multi Part Parameter/Volume",3,496,347,38,@"Volume");knob(@"Patch Common/Panorama",3,547,347,38,@"Pan");knob(@"Delay/Send",3,597,347,38);
 knob(@"Velocity Response/Volume",3,496,408,38,@"Vel → vol");knob(@"Velocity Response/Panorama",3,547,408,38,@"Vel → pan");knob(@"Reverb/Send",3,597,408,38);
 // LFO rows: rate/waveform/contour left, fixed destinations and assignment right.
 NSArray* shapes=@[@"Sine",@"Triangle",@"Saw",@"Square",@"S&H",@"S&G"];
 for(int lfo=1;lfo<=3;++lfo){double y=49+(lfo-1)*152;NSString* group=[NSString stringWithFormat:@"LFO %d",lfo];
  ring([group stringByAppendingString:@"/Rate"],4,260,y+59,76,@"Rate");knob([group stringByAppendingString:@"/Key Follow"],4,340,y+39,40,@"Key follow");
  choices([group stringByAppendingString:@"/Shape"],shapes,1);for(int s=0;s<6;++s)radio([group stringByAppendingString:@"/Shape"],4,412+(s/2)*32,y+28+(s%2)*14,30,12,s,shapes[s]);
  node(@"HorizontalKnob",[group stringByAppendingString:@"/Shape"],4,379,y+58,65,34,@{@"Label":@""});
  menu([group stringByAppendingString:@"/Clock"],4,471,y+73,32);button([group stringByAppendingString:@"/Poly Mode"],4,526,y+29,28,@"Poly");
  if(lfo<3){knob([group stringByAppendingString:@"/Trigger Phase"],4,340,y+116,40,@"Trigger phase");slider([group stringByAppendingString:@"/Contour"],4,377,y+118,127,@"Contour");button([group stringByAppendingString:@"/Envelope Mode"],4,526,y+103,28,@"Env");
   NSArray* targets=lfo==1?@[@"Osc1",@"Osc2",@"Pulse Width",@"Reso1+2",@"Filtergain"]:@[@"Cutoff1",@"Cutoff2",@"Shape1+2",@"Panorama",@"FM Amount"];
   for(int t=0;t<5;++t)knob([group stringByAppendingFormat:@"/Dest %@",targets[t]],4,608+t*60,y+40,40,targets[t]);
   knob([group stringByAppendingString:@"/Assign Amount"],4,690,y+115,40,@"Assign amount");menu([group stringByAppendingString:@"/Assign Target"],4,730,y+104,141);
  }else {knob(@"LFO 3/Fade In",4,608,y+64,40);knob(@"LFO 3/Assign Amount",4,690,y+64,40,@"Assign amount");menu(@"LFO 3/Assign Target",4,730,y+53,141);}
 }
 // Six modulation slots in two columns; amount sliders precede destinations.
 for(int slot=1;slot<=6;++slot){double x=slot<=3?211:550,y=49+((slot-1)%3)*141;NSString* group=[NSString stringWithFormat:@"Matrix %d",slot];
  choices([group stringByAppendingString:@"/Source"],@[@"Off"]);menu([group stringByAppendingString:@"/Source"],5,x+156,y+23,166);
  for(int dest=1;dest<=3;++dest){NSString* target=[group stringByAppendingFormat:@"/Destination %d",dest];choices(target,@[@"Off"]);menu(target,5,x+156,y+56+(dest-1)*24,166);slider([group stringByAppendingFormat:@"/Amount %d",dest],5,x+19,y+58+(dest-1)*24,130,@"");}
 }
 // Arp header and a deliberately read-only pattern area.
 choices(@"Arpeggiator/Mode",@[@"Off",@"Up",@"Down",@"Up / Down",@"As played",@"Random"]);choices(@"Arpeggiator/Pattern",@[@"User"]);choices(@"Arpeggiator/Octaves",@[@"1",@"2",@"3",@"4"]);
 choices(@"Arpeggiator/Clock",@[@"1/16"]);menu(@"Arpeggiator/Mode",6,221,87,106);menu(@"Arpeggiator/Pattern",6,351,87,106);menu(@"Arpeggiator/Octaves",6,483,87,56);menu(@"Arpeggiator/Clock",6,563,87,106);
 knob(@"Arpeggiator/NoteLength",6,731,97,40,@"Note length");knob(@"Arpeggiator/Swing",6,791,97,40);button(@"Arpeggiator/Hold",6,840,87,32,@"Hold");node(@"ArpEdit",@"Arpeggiator/Pattern",6,222,162,648,280);
 // Effects 1, retaining the character/distortion/input/follower and FX columns.
 choices(@"Character/Type",@[@"Analog Boost"]);menu(@"Character/Type",7,270,95,143);knob(@"Analog Boost/Intensity",7,239,105,40,@"Intensity");knob(@"Analog Boost/Tune",7,440,105,40,@"Tune");
 row(@[@"Patch Distortion/Mix",@"Patch Distortion/Intensity",@"Patch Distortion/Treble Booster",@"Patch Distortion/High Cut"],7,239,210,67,40);
 choices(@"Patch Distortion/Type",@[@"Off"]);menu(@"Patch Distortion/Type",7,270,251,143);
 for(int i=0;i<3;++i){NSString* name=@[@"Input/Mode",@"Input/Source",@"Input/Output"][i];choices(name,i==1?@[@"L + R"]:@[@"Off"]);menu(name,7,225+i*82,334,67);}
 knob(@"Envelope Follower/Level",7,239,422,40);choices(@"Envelope Follower/Mode",@[@"Off"]);menu(@"Envelope Follower/Mode",7,270,412,93);knob(@"Envelope Follower/Attack",7,389,422,40);knob(@"Envelope Follower/Release",7,440,422,40);
 knob(@"Chorus/Mix",7,504,105,40,@"Chorus mix");choices(@"Chorus/Type",@[@"Classic"]);menu(@"Chorus/Type",7,534,95,117);
 row(@[@"Chorus/Delay",@"Chorus/Feedback",@"Chorus/LFO Rate",@"Chorus/LFO Depth"],7,679,105,58,40);
 knob(@"Phaser/Mix",7,504,210,40,@"Mix");choices(@"Phaser/Stages",@[@"1",@"2",@"3",@"4",@"5",@"6"],3);menu(@"Phaser/Stages",7,535,200,58);
 row(@[@"Phaser/Frequency",@"Phaser/Feedback",@"Phaser/Spread",@"Phaser/Rate",@"Phaser/Depth"],7,629,210,50,40);
 knob(@"Filter Bank/Mix",7,504,317,40,@"Mix");choices(@"Filter Bank/Type",@[@"Off"]);menu(@"Filter Bank/Type",7,534,307,117);knob(@"Filter Bank/Frequency",7,682,317,40,@"Frequency");knob(@"Filter Bank/Stereo Phase",7,732,317,40,@"Stereo phase");
 row(@[@"EQ/Low Gain",@"EQ/Low Frequency",@"EQ/Mid Gain",@"EQ/Mid Frequency",@"EQ/Mid Q",@"EQ/High Gain",@"EQ/High Frequency"],7,504,422,58,37);
 // Effects 2: delay, reverb and vocoder each retain a horizontal panel.
 ring(@"Delay/Send",11,272,124,82,@"Delay send");choices(@"Delay/Type",@[@"Classic",@"Tape"]);menu(@"Delay/Type",11,322,80,166);
 choices(@"Delay/Mode",@[@"Simple delay"]);menu(@"Delay/Mode",11,330,109,158);
 for(int clock=0;clock<2;++clock){NSString* name=clock?@"Delay/Clock Right":@"Delay/Clock Left";parameter(name);choices(name,@[@"Off"]);menu(name,11,330+clock*84,134,80);nodes.lastObject[@"attrs"][@"ValuePrefix"]=clock?@"R: ":@"L: ";}
 row(@[@"Delay/Time",@"Delay/Feedback",@"Delay/Color",@"Delay/LFO Rate",@"Delay/LFO Depth"],11,518,122,60,40);choices(@"Delay/Shape",shapes,1);menu(@"Delay/Shape",11,788,111,85);
 ring(@"Reverb/Send",11,272,264,82,@"Reverb send");choices(@"Reverb/Type",@[@"Reverb"]);menu(@"Reverb/Type",11,322,221,166);choices(@"Reverb/Room",@[@"Large room"]);menu(@"Reverb/Room",11,322,283,166);
 row(@[@"Reverb/Time",@"Reverb/Damping",@"Reverb/Color",@"Reverb/Predelay",@"Reverb/Feedback"],11,518,262,60,40);parameter(@"Reverb/Clock");choices(@"Reverb/Clock",@[@"Off"]);menu(@"Reverb/Clock",11,788,251,85);
 choices(@"Vocoder/Mode",@[@"Off"]);menu(@"Vocoder/Mode",11,227,385,106);slider(@"Vocoder/Balance",11,359,371,127,@"Balance");slider(@"Vocoder/Spectral Balance",11,359,425,127,@"Spectral balance");knob(@"Vocoder/Attack",11,518,376,40);knob(@"Vocoder/Release",11,518,430,40);
 NSArray* vocoderFields=@[@"Carrier Center Frequency",@"Carrier Frequency Spread",@"Carrier Q Factor",@"Modulator Center Frequency",@"Modulator Frequency Offset",@"Modulator Q Factor"],*vocoderLabels=@[@"Carrier freq",@"Carrier span",@"Carrier Q",@"Mod freq",@"Mod offset",@"Mod Q"];
 for(int i=0;i<6;++i)knob([@"Vocoder/" stringByAppendingString:vocoderFields[i]],11,588+i*50,403,38,vocoderLabels[i]);
 // Common: transpose/key mode, output, bender, categories and soft-knob rows.
 knob(@"Patch Common/Transpose",9,258,100,40);knob(@"Patch Common/Detune",9,337,100,40);choices(@"Patch Common/Smooth Mode",@[@"On"]);choices(@"Patch Common/KeyMode",@[@"Poly",@"Mono"]);menu(@"Patch Common/Smooth Mode",9,398,80,106);menu(@"Patch Common/KeyMode",9,398,111,106);
 slider(@"Surround/Balance",9,576,93,114,@"Balance");choices(@"Multi Part Parameter/Output",@[@"USB1 L+R",@"USB2 L+R",@"USB3 L+R"]);menu(@"Multi Part Parameter/Output",9,738,91,106);
 knob(@"Pitch Bender/Down",9,258,193,40);knob(@"Pitch Bender/Up",9,337,193,40);choices(@"Pitch Bender/Scale",@[@"Exponential",@"Linear"]);menu(@"Pitch Bender/Scale",9,398,183,106);
 for(int i=1;i<=2;++i){NSString* category=[NSString stringWithFormat:@"Patch Common/Category %d",i];choices(category,@[@"Off"]);menu(category,9,576+(i-1)*162,183,106);}
 for(int i=1;i<=3;++i){NSString* target=[NSString stringWithFormat:@"Soft Knobs/Destination %d",i],*name=[NSString stringWithFormat:@"Soft Knobs/Name %d",i];choices(target,@[@"Off"]);choices(name,@[@">Parameter"]);menu(target,9,221+(i-1)*220,258,193);menu(name,9,221+(i-1)*220,281,193);}
 note(@"Remote template support is not available in this beta.",8,240,114,613,50);
 note(@"Native AU / VST3 · RAM recall and restoration\nUse Import Original… for the optional Access artwork.",12,240,114,613,70);
 return @{@"project_owned":@YES,@"classic_layout":@YES,@"nodes":nodes,@"parameters":parameters,@"conditions":conditions,@"valuelists":lists};
}
}
