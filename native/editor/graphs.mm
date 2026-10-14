#include "graphs.h"
#include "graph_templates.h"
#include "graph_waveforms.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace VirusRework::Graphs {
namespace {
double unit(double x) {return std::isfinite(x)?std::clamp(x,0.,1.):0.;}
double get(Read read,NSString* name,double divisor=127.) {return unit(read(name)/divisor);}
NSString* group(NSDictionary* attrs) {
 NSString* parameter=attrs[@"Parameter"]?:@"";
 NSRange slash=[parameter rangeOfString:@"/" options:NSBackwardsSearch];
 return slash.location==NSNotFound?parameter:[parameter substringToIndex:slash.location];
}
NSString* parameter(NSString* prefix,NSString* leaf) {return [prefix stringByAppendingFormat:@"/%@",leaf];}
bool envelope(NSString* type) {return [type isEqualToString:@"ADSTREnvelope"];}
bool classic(NSString* type) {return [type isEqualToString:@"OscClassicShape"];}
bool hyper(NSString* type) {return [type isEqualToString:@"OscHypersawShape"];}
bool filter(NSString* type) {return [@[@"LP4Response",@"HP4Response",@"BP4Response",@"BS4Response"] containsObject:type];}
struct Segment {
 TemplateNode spec{};
 NSPoint start{},end{};
 __strong NSString* xName=nil;
 __strong NSString* yName=nil;
};
std::vector<Segment> segments(NSString* type,NSDictionary* attrs,Read read) {
 const TemplateNode* table=nullptr;std::size_t count=0;
 if(envelope(type)){table=ENV;count=6;}
 else if([type isEqualToString:@"LP4Response"]){table=LP;count=4;}
 else if([type isEqualToString:@"HP4Response"]){table=HP;count=4;}
 else if([type isEqualToString:@"BP4Response"]){table=BP;count=3;}
 else if([type isEqualToString:@"BS4Response"]){table=BS;count=5;}
 if(!table)return {};
 std::vector<Segment> result(count);
 NSString* prefix=group(attrs);
 for(std::size_t i=0;i<count;++i){
  result[i].spec=table[i];
  if(envelope(type)) {
   if(i==1)result[i].xName=parameter(prefix,@"Attack");
   if(i==2)result[i].xName=parameter(prefix,@"Decay");
   if(i==3)result[i].yName=parameter(prefix,@"Sustain");
   if(i==4)result[i].yName=parameter(prefix,@"Sustain Time");
   if(i==5)result[i].xName=parameter(prefix,@"Release");
  } else if(table[i].bx) {
   result[i].xName=attrs[@"Parameter"];
   result[i].yName=parameter(prefix,@"Resonance");
  }
  if(result[i].xName)result[i].spec.x=get(read,result[i].xName);
  if(result[i].yName)result[i].spec.y=get(read,result[i].yName);
 }
 return result;
}
// Recovered KEnvelope logical bounds (0x682fe); both axes use table offsets and
// full normalized input extent, so changing one parameter never rescales others.
std::pair<double,double> bounds(const std::vector<Segment>& nodes,bool x) {
 double low=0,high=0,minimum=0,maximum=0;
 for(const auto& node:nodes){const auto& n=node.spec;int mode=x?n.xm:n.ym;
  const double offset=x?n.xoff:n.yoff,scale=x?n.xscale:n.yscale;
  if(mode==1){minimum=std::min(minimum,low);maximum=std::max(maximum,high);low=offset;high=offset+scale;}
  else if(mode!=3){low+=offset;high+=offset+scale;}
 }
 return {std::min(minimum,low),std::max(maximum,high)};
}
void locate(std::vector<Segment>& nodes,NSRect rect) {
 const auto xb=bounds(nodes,true),yb=bounds(nodes,false);
 // Assembly 0x6967a (Ghidra lacks this function entry) propagates mode-2
 // endpoints backwards and clamps mode-3 segments before pixel conversion.
 for(std::size_t i=0;i<nodes.size();++i){auto& n=nodes[i];const auto& s=n.spec;
  n.end={s.x*s.xscale,-s.y*s.yscale};
  if(i){n.start=nodes[i-1].end;
   if(s.xm==1||s.xm==2){n.end.x-=xb.first;
    if(s.xm==2)for(std::size_t j=i;j&&nodes[j].spec.xm==2;--j){const double v=nodes[j].end.x-(j==i?0:nodes[j].spec.xoff);nodes[j].start.x=v;nodes[j-1].end.x=v;}
   }else n.end.x+=n.start.x;
   if(s.ym==1||s.ym==2){n.end.y+=yb.first;
    if(s.ym==2)for(std::size_t j=i;j&&nodes[j].spec.ym==2;--j){const double v=nodes[j].end.y+(j==i?0:nodes[j].spec.yoff);nodes[j].start.y=v;nodes[j-1].end.y=v;}
   }else n.end.y+=n.start.y;
  }
  n.end.x+=s.xoff;n.end.y-=s.yoff;
  if(i&&s.xm==3){const auto& p=nodes[i-1].spec;const double low=p.xoff-xb.first;n.end.x=std::clamp(n.end.x,low,low+p.xscale);}
  if(i&&s.ym==3){const auto& p=nodes[i-1].spec;const double high=yb.first-p.yoff;n.end.y=std::clamp(n.end.y,high-p.yscale,high);}
 }
 const double sx=(rect.size.width-7)/(xb.second-xb.first),sy=(rect.size.height-7)/(yb.second-yb.first);
 for(auto& n:nodes)for(NSPoint* p:{&n.start,&n.end}){p->x=rect.origin.x+p->x*sx+3;p->y=NSMaxY(rect)+p->y*sy-4;}
}
double shaped(double t,double curve) {
 // Closed form of recovered powf(1000,c) recurrence in 0x69c1c.
 if(std::abs(curve)<.001)return t;
 return std::expm1(std::log(1000.)*curve*t)/std::expm1(std::log(1000.)*curve);
}
bool valid(NSRect rect,Read read) {return read&&std::isfinite(rect.origin.x)&&std::isfinite(rect.origin.y)&&std::isfinite(rect.size.width)&&std::isfinite(rect.size.height)&&rect.size.width>=8&&rect.size.height>=8&&rect.size.width<=4096&&rect.size.height<=4096;}
}
bool supports(NSString* type){return envelope(type)||classic(type)||hyper(type)||filter(type);}
Geometry geometry(NSString* type,NSDictionary* attrs,NSRect rect,Read read){
 Geometry out;if(!supports(type)||!valid(rect,read))return out;
 NSString* prefix=group(attrs);
 if(classic(type)){
  const double shape=get(read,parameter(prefix,@"Shape")),pw=get(read,attrs[@"Parameter"]);
  const auto wave=static_cast<std::size_t>(std::round(get(read,parameter(prefix,@"WaveSelect"),63)*63));
  const double wavemix=std::max(0.,1-2*shape),sawmix=1-std::abs(2*shape-1),pulsemix=std::max(0.,2*shape-1);
  const int duty=static_cast<int>((pw*.5+.5)*64);
  const double middle=std::floor(rect.size.height*.5),center=rect.origin.y+middle;
  std::vector<NSPoint> line;line.reserve(66);line.push_back({rect.origin.x,center});
  for(std::size_t i=0;i<64;++i){float sample;const auto bits=waveformBits[wave][i];std::memcpy(&sample,&bits,sizeof sample);
   const double y=std::clamp(sample*wavemix+(1-static_cast<double>(i)/32)*sawmix+(static_cast<int>(i)<duty?pulsemix:-pulsemix),-1.,1.);
   line.push_back({rect.origin.x+i*rect.size.width/64,center-y*middle});
  }
  line.push_back({NSMaxX(rect)-1,center});out.paths.push_back(std::move(line));out.fillToBaseline=true;
  out.handles.push_back({{rect.origin.x+pw*rect.size.width,NSMaxY(rect)-shape*rect.size.height},attrs[@"Parameter"],parameter(prefix,@"Shape")});
 }else if(hyper(type)){
  const auto density=static_cast<int>(std::round(get(read,parameter(prefix,@"Density"))*127));
  constexpr int thresholds[]={32,64,80,96,104,112,120,128};int region=0,previous=0;
  while(region<7&&density>=thresholds[region])previous=thresholds[region++];
  const int voices=region+2;const double fraction=density==127?1.:(density-previous)/static_cast<double>(thresholds[region]-previous);
  const double detune=get(read,attrs[@"Parameter"]),spacing=(rect.size.width/9-2)*detune+2;
  for(int i=0;i<voices;++i){const double sign=i%2?1:-1;const double x=std::trunc(rect.origin.x+rect.size.width*.5+((i+1)/2)*sign*spacing);const double height=rect.size.height*(i==voices-1?fraction:1);out.paths.push_back({{x,std::trunc(NSMaxY(rect))},{x,std::trunc(NSMaxY(rect)-height)}});}
  out.handles.push_back({{rect.origin.x+detune*rect.size.width,NSMaxY(rect)-density/127.*rect.size.height},attrs[@"Parameter"],parameter(prefix,@"Density")});
 }else{
  auto model=segments(type,attrs,read);locate(model,rect);
  std::vector<NSPoint> line;
  for(std::size_t i=1;i<model.size();++i){const auto& n=model[i];
   const std::size_t steps=std::clamp<std::size_t>(static_cast<std::size_t>(std::ceil(std::abs(n.end.x-n.start.x))),1,4096);
   if(line.empty())line.push_back(n.start);
   for(std::size_t s=1;s<=steps;++s){double t=static_cast<double>(s)/steps;line.push_back({n.start.x+(n.end.x-n.start.x)*t,n.start.y+(n.end.y-n.start.y)*shaped(t,n.spec.curve)});}
   if(n.xName||n.yName)out.handles.push_back({n.end,n.xName,n.yName});
  }
  out.paths.push_back(std::move(line));out.fillToBaseline=true;
 }
 return out;
}
void draw(NSString* type,NSDictionary* attrs,NSRect rect,Read read,Image image){
 const auto g=geometry(type,attrs,rect,read);if(g.paths.empty())return;
 [NSGraphicsContext saveGraphicsState];NSRectClip(rect);[NSGraphicsContext currentContext].shouldAntialias=YES;
 const bool oscillator=classic(type)||hyper(type);
 for(const auto& points:g.paths){if(points.empty())continue;NSBezierPath* path=[NSBezierPath bezierPath];[path moveToPoint:points.front()];for(std::size_t i=1;i<points.size();++i)[path lineToPoint:points[i]];
  if(g.fillToBaseline){NSBezierPath* fill=[path copy];const double baseline=classic(type)?rect.origin.y+std::floor(rect.size.height*.5):NSMaxY(rect)-4;[fill lineToPoint:{points.back().x,baseline}];[fill lineToPoint:{points.front().x,baseline}];[fill closePath];
   if(oscillator)[[NSColor colorWithWhite:0 alpha:.65]setFill];else [[NSColor colorWithRed:.75 green:.28 blue:.28 alpha:.24]setFill];[fill fill];
  }
  if(oscillator)[NSColor.whiteColor setStroke];else [[NSColor colorWithRed:1 green:95./255 blue:95./255 alpha:1]setStroke];path.lineWidth=1;[path stroke];
 }
 if(!oscillator)for(const auto& handle:g.handles){NSRect r=NSMakeRect(std::round(handle.point.x)-3,std::round(handle.point.y)-3,7,7);NSImage* sprite=image?image(attrs[@"NormalHandle"]):nil;
  if(sprite)[sprite drawInRect:r fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];
  else{[[NSColor colorWithRed:1 green:95./255 blue:95./255 alpha:1]setFill];NSRectFill(r);[NSColor.blackColor setFill];NSRectFill(NSInsetRect(r,2,2));}
 }
 [NSGraphicsContext restoreGraphicsState];
}
NSInteger hit(NSString* type,NSDictionary* attrs,NSRect rect,Read read,NSPoint point){
 if(!std::isfinite(point.x)||!std::isfinite(point.y)||!NSPointInRect(point,rect))return -1;
 const auto g=geometry(type,attrs,rect,read);if(g.handles.empty())return -1;
 if(classic(type)||hyper(type))return 0;
 NSInteger nearest=-1;double best=81;
 for(std::size_t i=0;i<g.handles.size();++i){const auto p=g.handles[i].point;double dx=p.x-point.x,dy=p.y-point.y,d=dx*dx+dy*dy;if(d<=best){best=d;nearest=static_cast<NSInteger>(i);}}
 return nearest;
}
NSArray<NSString*>* editingParameters(NSString* type,NSDictionary* attrs,NSRect rect,Read read,NSInteger index){
 const auto g=geometry(type,attrs,rect,read);if(index<0||static_cast<std::size_t>(index)>=g.handles.size())return @[];
 NSMutableArray* names=[NSMutableArray array];const auto& h=g.handles[index];if(h.xParameter)[names addObject:h.xParameter];if(h.yParameter&&![names containsObject:h.yParameter])[names addObject:h.yParameter];return names;
}
NSArray<NSDictionary*>* edits(NSString* type,NSDictionary* attrs,NSRect rect,Read read,NSInteger index,NSPoint point){
 const auto g=geometry(type,attrs,rect,read);if(index<0||static_cast<std::size_t>(index)>=g.handles.size()||!std::isfinite(point.x)||!std::isfinite(point.y))return @[];
 const auto& h=g.handles[index];NSMutableArray* result=[NSMutableArray array];
 for(int axis=0;axis<2;++axis){NSString* name=axis?h.yParameter:h.xParameter;if(!name)continue;
  auto coordinate=[&](double value){Read changed=^double(NSString* key){return [name isEqualToString:key]?value:read(key);};const auto model=geometry(type,attrs,rect,changed);return axis?model.handles[index].point.y:model.handles[index].point.x;};
  const double at0=coordinate(0),at127=coordinate(127),target=axis?point.y:point.x;
  if(std::abs(at127-at0)<1e-9)continue;
  double low=0,high=127;bool increasing=at127>at0;
  for(int pass=0;pass<24;++pass){double mid=(low+high)*.5,c=coordinate(mid);if((c<target)==increasing)low=mid;else high=mid;}
  // A quantized coordinate (HyperSaw density) has half-integer plateaus.
  // Choose the closer integer endpoint instead of rounding a plateau boundary.
  const double lower=std::floor((low+high)*.5),upper=std::ceil((low+high)*.5);
  const double value=std::abs(coordinate(lower)-target)<=std::abs(coordinate(upper)-target)?lower:upper;
  [result addObject:@{@"parameter":name,@"value":@(value)}];
 }
 return result;
}
} // namespace VirusRework::Graphs
