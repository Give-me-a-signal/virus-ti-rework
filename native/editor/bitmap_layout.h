#pragma once
#import <Cocoa/Cocoa.h>
#include <algorithm>

namespace VirusRework::Artwork {
// XML geometry and filmstrip offsets are pixels, independent of PNG print DPI.
inline NSImage* load(NSString* path) {
 NSBitmapImageRep* rep=[[NSBitmapImageRep alloc]initWithData:[NSData dataWithContentsOfFile:path]];
 if(!rep)return nil;
 NSSize pixels=NSMakeSize(rep.pixelsWide,rep.pixelsHigh);rep.size=pixels;
 NSImage* image=[[NSImage alloc]initWithSize:pixels];[image addRepresentation:rep];return image;
}
inline NSEdgeInsets insets(NSDictionary* bitmap) {
 for(NSDictionary* declaration in bitmap[@"declarations"])
  if([declaration[@"attrs"][@"Tile"] isEqual:@"true"])
   for(NSDictionary* child in declaration[@"children"])if([child[@"type"] isEqual:@"TileInsetRect"]){NSDictionary* a=child[@"attrs"];return NSEdgeInsetsMake([a[@"top"]doubleValue],[a[@"left"]doubleValue],[a[@"bottom"]doubleValue],[a[@"right"]doubleValue]);}
 return NSEdgeInsetsMake(0,0,0,0);
}
inline bool sliced(NSEdgeInsets i){return i.left+i.right+i.top+i.bottom>0;}
inline void draw(NSImage* image,NSRect destination,NSRect source,NSEdgeInsets cap) {
 if(!image||destination.size.width<=0||destination.size.height<=0)return;
 auto piece=[&](NSRect d,NSRect s){if(d.size.width>0&&d.size.height>0&&s.size.width>0&&s.size.height>0)[image drawInRect:d fromRect:s operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:@{NSImageHintInterpolation:@(NSImageInterpolationNone)}];};
 if(!sliced(cap)){piece(destination,source);return;}
 double l=cap.left,r=cap.right,t=cap.top,b=cap.bottom;
 const double xscale=std::min(1.,destination.size.width/std::max(1.,l+r));
 const double yscale=std::min(1.,destination.size.height/std::max(1.,t+b));
 double dx[]={0,l*xscale,destination.size.width-r*xscale,destination.size.width};
 double dy[]={0,t*yscale,destination.size.height-b*yscale,destination.size.height};
 double sx[]={0,l,source.size.width-r,source.size.width};
 double sy[]={0,t,source.size.height-b,source.size.height};
 for(int y=0;y<3;++y)for(int x=0;x<3;++x)
  piece(NSMakeRect(destination.origin.x+dx[x],destination.origin.y+dy[y],dx[x+1]-dx[x],dy[y+1]-dy[y]),NSMakeRect(source.origin.x+sx[x],NSMaxY(source)-sy[y+1],sx[x+1]-sx[x],sy[y+1]-sy[y]));
}
inline NSRect menuTextRect(NSRect r,NSEdgeInsets cap) {
 double l=sliced(cap)?cap.left:1,rgt=sliced(cap)?cap.right:1;
 r.origin.x+=l;r.size.width=std::max(0.,r.size.width-l-rgt);
 r.origin.y+=cap.top;r.size.height=std::max(0.,r.size.height-cap.top-cap.bottom);
 return r;
}
}
