#include "graphs.h"
#include "illustrative_waves.h"
#include <algorithm>
#include <cmath>
namespace VirusRework::Graphs {
namespace {
double normalized(Read read,NSString* name,double fallback=64.) {double v=read?read(name):fallback;return std::isfinite(v)?std::clamp(v/127.,0.,1.):fallback/127.;}
NSString* prefix(NSDictionary* attrs){NSString* n=attrs[@"Parameter"]?:@"";NSRange slash=[n rangeOfString:@"/" options:NSBackwardsSearch];return slash.location==NSNotFound?n:[n substringToIndex:slash.location];}
NSString* field(NSString* group,NSString* name){return [group stringByAppendingFormat:@"/%@",name];}
}
bool supports(NSString* type){return [@[@"ADSTREnvelope",@"LP4Response",@"HP4Response",@"BP4Response",@"BS4Response",@"OscClassicShape",@"OscHypersawShape"]containsObject:type];}
Geometry geometry(NSString* type,NSDictionary* attrs,NSRect r,Read read){Geometry g;if(!supports(type)||!read||!std::isfinite(r.origin.x)||!std::isfinite(r.origin.y)||!std::isfinite(r.size.width)||!std::isfinite(r.size.height)||r.size.width<8||r.size.height<8)return g;
 NSString* group=prefix(attrs);double left=NSMinX(r)+3,right=NSMaxX(r)-3,top=NSMinY(r)+3,bottom=NSMaxY(r)-3,w=right-left,h=bottom-top;
 std::vector<NSPoint> path;
 if([type isEqual:@"ADSTREnvelope"]){double a=normalized(read,field(group,@"Attack")),d=normalized(read,field(group,@"Decay")),s=normalized(read,field(group,@"Sustain")),release=normalized(read,field(group,@"Release"));
  double xa=left+w*(.02+.24*a),xd=xa+w*(.03+.24*d),xr=right-w*(.03+.22*release),ys=bottom-h*s;
  path={{left,bottom},{xa,top},{xd,ys},{std::max(xd,xr),ys},{right,bottom}};
 }else if([type isEqual:@"OscClassicShape"]){double shape=normalized(read,field(group,@"Shape")),pulse=normalized(read,attrs[@"Parameter"]);for(int i=0;i<=128;++i){double x=i/128.;double wave=std::sin(x*6.283185307179586),saw=1-2*x,square=x<.5+.45*pulse?1:-1;
   double y=shape<.5?wave*(1-2*shape)+saw*2*shape:saw*(2-2*shape)+square*(2*shape-1);path.push_back({left+w*x,top+h*(1-y)/2});}
 }else if([type isEqual:@"OscHypersawShape"]){double density=normalized(read,field(group,@"Density")),spread=normalized(read,attrs[@"Parameter"]);int count=1+int(std::round(density*7));for(int i=0;i<count;++i){double x=left+w*(.5+(i-(count-1)/2.)*spread/8.);g.paths.push_back({{x,bottom},{x,top+h*.1*std::abs(i-(count-1)/2.)}});}return g;
 }else {double cutoff=.04+.92*normalized(read,attrs[@"Parameter"]),resonance=normalized(read,field(group,@"Resonance"));for(int i=0;i<=128;++i){double x=i/128.,low=1/std::sqrt(1+std::pow(std::max(x,.001)/cutoff,8));double magnitude=low;
   if([type isEqual:@"HP4Response"])magnitude=1-low;
   if([type isEqual:@"BP4Response"])magnitude=std::exp(-std::pow((x-cutoff)/(.09+.15*(1-resonance)),2));
   if([type isEqual:@"BS4Response"])magnitude=1-std::exp(-std::pow((x-cutoff)/(.06+.12*(1-resonance)),2));
   magnitude=std::clamp(magnitude+.15*resonance*std::exp(-std::pow((x-cutoff)/.04,2)),0.,1.);path.push_back({left+w*x,bottom-h*magnitude});}
 }
 g.paths.push_back(std::move(path));return g;
}
void draw(NSString* type,NSDictionary* attrs,NSRect r,Read read,Image){auto g=geometry(type,attrs,r,read);[NSGraphicsContext saveGraphicsState];[[NSBezierPath bezierPathWithRect:r]addClip];for(auto& points:g.paths){auto p=[NSBezierPath bezierPath];bool first=true;for(auto point:points){if(first)[p moveToPoint:point];else[p lineToPoint:point];first=false;}[[NSColor colorWithSRGBRed:.9 green:.25 blue:.3 alpha:1]setStroke];p.lineWidth=1.1;[p stroke];}[NSGraphicsContext restoreGraphicsState];}
// These are illustrative displays. Hardware editing remains on verified controls.
NSInteger hit(NSString*,NSDictionary*,NSRect,Read,NSPoint){return -1;}
NSArray<NSString*>* editingParameters(NSString*,NSDictionary*,NSRect,Read,NSInteger){return @[];}
NSArray<NSDictionary*>* edits(NSString*,NSDictionary*,NSRect,Read,NSInteger,NSPoint){return @[];}
}
