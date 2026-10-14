#pragma once
#import <Cocoa/Cocoa.h>
#include <algorithm>
#include <cmath>
namespace VirusRework::Skin {
inline NSColor* rgb(unsigned hex){return [NSColor colorWithSRGBRed:((hex>>16)&255)/255. green:((hex>>8)&255)/255. blue:(hex&255)/255. alpha:1];}
struct Palette {
 bool dark;
 NSColor *base,*panel,*raised,*line,*text,*muted,*accent,*display,*ink;
 explicit Palette(bool d):dark(d),base(rgb(d?0x17191d:0xe9eae6)),panel(rgb(d?0x22252a:0xf9f9f5)),raised(rgb(d?0x2d3137:0xffffff)),line(rgb(d?0x3c4148:0xc9cdc9)),text(rgb(d?0xe9eae5:0x272d30)),muted(rgb(d?0xa8afb5:0x606970)),accent(rgb(d?0xf15b61:0xbe2938)),display(rgb(d?0x111519:0xe4e8e5)),ink(rgb(d?0xdce7e1:0x243b39)){}
};
inline void box(NSRect r,NSColor* fill,NSColor* stroke=nil,CGFloat radius=4){auto p=[NSBezierPath bezierPathWithRoundedRect:r xRadius:radius yRadius:radius];[fill setFill];[p fill];if(stroke){[stroke setStroke];p.lineWidth=.65;[p stroke];}}
inline void text(NSString* s,NSRect r,CGFloat size,NSColor* color,BOOL center=NO,NSFontWeight weight=NSFontWeightRegular){
 if(!s.length)return;NSMutableParagraphStyle* style=[[NSMutableParagraphStyle alloc]init];style.alignment=center?NSTextAlignmentCenter:NSTextAlignmentLeft;style.lineBreakMode=NSLineBreakByTruncatingTail;
 NSFont* font=[NSFont systemFontOfSize:size weight:weight];r.origin.y+=std::max(0.,(r.size.height-(font.ascender-font.descender))/2.-1);[s drawInRect:r withAttributes:@{NSFontAttributeName:font,NSForegroundColorAttributeName:color,NSParagraphStyleAttributeName:style}];
}
inline void line(NSPoint a,NSPoint b,NSColor* color,CGFloat width=1){auto p=[NSBezierPath bezierPath];[p moveToPoint:a];[p lineToPoint:b];p.lineWidth=width;[color setStroke];[p stroke];}
inline void arc(NSPoint c,double radius,double fraction,NSColor* color,double width){
 auto p=[NSBezierPath bezierPath];for(int n=0;n<=80;n++){double angle=(135+270*fraction*n/80.)*M_PI/180.;NSPoint q=NSMakePoint(c.x+radius*std::cos(angle),c.y+radius*std::sin(angle));if(n)[p lineToPoint:q];else[p moveToPoint:q];}p.lineWidth=width;p.lineCapStyle=NSLineCapStyleRound;[color setStroke];[p stroke];
}
inline NSString* label(NSString* parameter){
 NSDictionary* qualified=@{@"Delay/Send":@"Delay send",@"Reverb/Send":@"Reverb send",@"Chorus/Mix":@"Chorus mix",@"Oscillator 1/Volume":@"Osc 1",@"Oscillator 2/Volume":@"Osc 2",@"Oscillator 3/Volume":@"Osc 3",@"Noise/Volume":@"Noise",@"Noise Oscillator/Volume":@"Noise",@"Suboscillator/Volume":@"Sub osc",@"Sub Oscillator/Volume":@"Sub osc",@"Ring Modulator/Volume":@"Ring mod"};
 if(qualified[parameter])return qualified[parameter];
 if([parameter hasPrefix:@"Velocity Response/"]){NSDictionary* names=@{@"Pulse Width":@"Vel → PW",@"Osc1 Shape":@"Vel → osc 1",@"Osc2 Shape":@"Vel → osc 2",@"FM Amount":@"Vel → FM",@"Volume":@"Vel → vol",@"Panorama":@"Vel → pan"};if(names[parameter.lastPathComponent])return names[parameter.lastPathComponent];if([parameter containsString:@"Envelope"])return @"Vel → env";if([parameter containsString:@"Resonance"])return @"Vel → reso";return [@"Vel → " stringByAppendingString:parameter.lastPathComponent];}
 NSString* s=parameter.lastPathComponent?:@"";
 if([s hasPrefix:@"Dest "])s=[s substringFromIndex:5];
 NSDictionary* compact=@{@"Assign Amount":@"Assign amount",@"Assign Target":@"Assign",@"FilterEnv > Pitch":@"Env → pitch",@"FilterEnv > FM":@"Env → FM",@"Volume+Saturation":@"Osc vol / sat",@"Osc1 Shape":@"Osc 1 shape",@"Osc2 Shape":@"Osc 2 shape",@"Pan Spread":@"Pan spread",@"Pulsewidth":@"Pulse width",@"Punch Intensity":@"Intensity",@"Poly Mode":@"Poly",@"Envelope Mode":@"Env",@"Switch":@"Clock",@"Filtergain":@"Filter gain",@"Filter 1 Envelope Amount":@"Vel → env",@"Filter 2 Envelope Amount":@"Vel → env"};if(compact[s])return compact[s];
 NSDictionary* shortNames=@{@"Key Follow":@"Key follow",@"Envelope Amount":@"Env amount",@"Semitone":@"Semitone",@"Panorama":@"Pan",@"Ring Modulator Volume":@"Ring mod",@"Oscillator Volume":@"Osc volume",@"Sub Oscillator Volume":@"Sub osc",@"Velocity":@"Velocity",@"Filter Envelope Amount":@"Env amount",@"Pulse Width":@"Pulse width",@"Phase Init":@"Phase init"};
 return shortNames[s]?:[s stringByReplacingOccurrencesOfString:@"Oscillator" withString:@"Osc"];
}
inline void knob(NSRect r,double value,const Palette& p,bool hollow=false,bool enabled=true){
 const double side=std::min(r.size.width,r.size.height),radius=side*.40;NSPoint c=NSMakePoint(NSMidX(r),NSMidY(r));
 if(!hollow){auto circle=[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(c.x-radius,c.y-radius,radius*2,radius*2)];[p.raised setFill];[circle fill];[p.line setStroke];circle.lineWidth=.8;[circle stroke];}
 arc(c,side*.46,1,p.line,side>65?3:1.5);arc(c,side*.46,std::clamp(value,0.,1.),enabled?p.accent:p.muted,side>65?3:1.5);
 double angle=(135+270*value)*M_PI/180.;double inner=hollow?side*.37:side*.12;
 line(NSMakePoint(c.x+inner*cos(angle),c.y+inner*sin(angle)),NSMakePoint(c.x+side*.36*cos(angle),c.y+side*.36*sin(angle)),p.text,side>65?2.5:1.8);
}
}
