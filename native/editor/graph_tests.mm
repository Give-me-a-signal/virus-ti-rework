#import "graphs.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace VirusRework::Graphs;
static bool near(double a,double b){return std::abs(a-b)<1e-7;}
int main(){@autoreleasepool{
 NSMutableDictionary* values=[NSMutableDictionary dictionary];
 Read read=^double(NSString* name){return values[name]?[values[name] doubleValue]:64.;};
 NSArray* types=@[@"ADSTREnvelope",@"LP4Response",@"HP4Response",@"BP4Response",@"BS4Response",@"OscClassicShape",@"OscHypersawShape"];
 NSArray* names=@[@"Amp Envelope/Attack",@"Filter 1/Cutoff",@"Filter 1/Cutoff",@"Filter 1/Cutoff",@"Filter 1/Cutoff",@"Oscillator 1/Pulsewidth",@"Oscillator 1 HyperSaw/DetuneSpread"];
 for(NSUInteger n=0;n<types.count;++n){
  NSString* type=types[n];NSDictionary* attrs=@{@"Parameter":names[n]};NSRect r=NSMakeRect(40,80,225,80);
  auto base=geometry(type,attrs,r,read);assert(!base.paths.empty());assert(!base.handles.empty());
  for(NSUInteger h=0;h<base.handles.size();++h){
   auto editNames=editingParameters(type,attrs,r,read,h);assert(editNames.count);
   for(NSString* parameterName in editNames){
    for(int value=0;value<=127;++value){
     values[parameterName]=@(value);auto g=geometry(type,attrs,r,read);
     for(const auto& path:g.paths)for(NSPoint p:path){assert(std::isfinite(p.x)&&std::isfinite(p.y));assert(p.x>=NSMinX(r)-1&&p.x<=NSMaxX(r)+1);assert(p.y>=NSMinY(r)-1&&p.y<=NSMaxY(r)+1);}
     const auto& handle=g.handles[h];assert(hit(type,attrs,r,read,handle.point)>=0 || !NSPointInRect(handle.point,r));
     NSArray* result=edits(type,attrs,r,read,h,handle.point);
     for(NSDictionary* edit in result)if([edit[@"parameter"] isEqualToString:parameterName]){
      // Saturated sustain-time segments can map several parameter values to one point.
      NSMutableDictionary* changed=[values mutableCopy];changed[parameterName]=edit[@"value"];
      Read inverse=^double(NSString* key){return changed[key]?[changed[key] doubleValue]:64.;};
      const auto restored=geometry(type,attrs,r,inverse).handles[h].point;
      if(!near(restored.x,handle.point.x)||!near(restored.y,handle.point.y)){fprintf(stderr,"%s %s input=%d inverse=%g handle=(%g,%g) restored=(%g,%g)\n",[type UTF8String],[parameterName UTF8String],value,[edit[@"value"] doubleValue],handle.point.x,handle.point.y,restored.x,restored.y);abort();}
     }
    }
    [values removeObjectForKey:parameterName];
   }
  }
  assert(edits(type,attrs,r,read,-1,NSZeroPoint).count==0);
  assert(edits(type,attrs,r,read,999,NSZeroPoint).count==0);
  assert(hit(type,attrs,r,read,NSMakePoint(NAN,0))==-1);
  Read invalid=^double(NSString*){return NAN;};auto safe=geometry(type,attrs,r,invalid);
  for(const auto& path:safe.paths)for(NSPoint p:path)assert(std::isfinite(p.x)&&std::isfinite(p.y));
 }
 // Recovered classic formula: Shape midpoint is saw; maximum is a pulse.
 NSDictionary* classicAttrs=@{@"Parameter":@"Oscillator 1/Pulsewidth"};NSRect r=NSMakeRect(0,0,64,32);
 values[@"Oscillator 1/Shape"]=@63.5;auto saw=geometry(@"OscClassicShape",classicAttrs,r,read);assert(saw.paths[0].size()==66);assert(near(saw.paths[0][1].y,0));assert(near(saw.paths[0][33].y,16));
 values[@"Oscillator 1/Shape"]=@127;values[@"Oscillator 1/Pulsewidth"]=@0;auto pulse=geometry(@"OscClassicShape",classicAttrs,r,read);assert(near(pulse.paths[0][32].y,0));assert(near(pulse.paths[0][33].y,32));
 values[@"Oscillator 1/Shape"]=@0;
 for(int wave=0;wave<64;++wave){values[@"Oscillator 1/WaveSelect"]=@(wave);auto g=geometry(@"OscClassicShape",classicAttrs,r,read);for(auto p:g.paths[0])assert(std::isfinite(p.y)&&p.y>=0&&p.y<=32);}
 // Density threshold crossings add one line while preserving the old line at full height.
 NSDictionary* hyperAttrs=@{@"Parameter":@"Oscillator 1 HyperSaw/DetuneSpread"};
 const int thresholds[]={0,32,64,80,96,104,112,120,127};
 for(int i=0;i<9;++i){values[@"Oscillator 1 HyperSaw/Density"]=@(thresholds[i]);auto g=geometry(@"OscHypersawShape",hyperAttrs,NSMakeRect(0,0,64,39),read);assert(g.paths.size()==static_cast<size_t>(std::min(i+2,9)));assert(g.paths.back()[0].y-g.paths.back()[1].y==(i==8?39:0));}
 assert(geometry(@"unknown",@{},r,read).paths.empty());assert(geometry(@"ADSTREnvelope",@{},NSZeroRect,read).paths.empty());
 std::puts("graph geometry, 128-step parameter/drag sweeps, 64 spectral waves and HyperSaw thresholds passed");
}}
