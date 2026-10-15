#import "graphs.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace VirusRework::Graphs;
int main(){@autoreleasepool{
 NSMutableDictionary* values=[NSMutableDictionary dictionary];Read read=^double(NSString* name){return values[name]?[values[name]doubleValue]:64.;};
 NSArray* types=@[@"ADSTREnvelope",@"LP4Response",@"HP4Response",@"BP4Response",@"BS4Response",@"OscClassicShape",@"OscHypersawShape"];
 for(NSString* type in types)for(int scale:{1,2})for(int value=0;value<128;++value){NSString* name=[type isEqual:@"ADSTREnvelope"]?@"Amp Envelope/Attack":[type hasPrefix:@"Osc"]?@"Oscillator 1/Shape":@"Filter 1/Cutoff";values[name]=@(value);NSRect r=NSMakeRect(40,80,225*scale,80*scale);auto g=geometry(type,@{@"Parameter":name},r,read);assert(!g.paths.empty());for(auto& path:g.paths)for(auto p:path){assert(std::isfinite(p.x)&&std::isfinite(p.y));assert(NSPointInRect(p,r));}assert(g.handles.empty());}
 NSRect r=NSMakeRect(0,0,225,80);NSDictionary* a=@{@"Parameter":@"Filter 1/Cutoff"};values[@"Filter 1/Cutoff"]=@10;auto low=geometry(@"LP4Response",a,r,read);values[@"Filter 1/Cutoff"]=@110;auto high=geometry(@"LP4Response",a,r,read);assert(low.paths[0][64].y>high.paths[0][64].y);
 assert(geometry(@"unknown",a,r,read).paths.empty());assert(geometry(@"LP4Response",a,NSZeroRect,read).paths.empty());assert(hit(@"LP4Response",a,r,read,NSZeroPoint)==-1);assert(edits(@"LP4Response",a,r,read,0,NSZeroPoint).count==0);
 puts("Illustrative graphs: parameter sweeps, bounds, scale, dynamics and read-only behavior passed");
}return 0;}
