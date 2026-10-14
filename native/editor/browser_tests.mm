#import "browser.h"
#include <cassert>
#include <iostream>
@interface VTRPatchBrowserView (Testing)
- (void)bankChosen:(NSMenuItem*)item;
- (void)categoryChosen:(NSMenuItem*)item;
- (void)controlTextDidChange:(NSNotification*)note;
@end
@interface BrowserScrollDouble : NSObject
@property CGFloat scrollingDeltaY;
@property NSPoint locationInWindow;
@end
@implementation BrowserScrollDouble
@end
int main(int argc,char**argv){assert(argc==3);@autoreleasepool{
 [NSApplication sharedApplication];__block NSDictionary* loaded=nil;
 VTRPatchBrowserView* view=[[VTRPatchBrowserView alloc]initWithFrame:NSMakeRect(0,0,705,426) resources:@(argv[1]) bankDirectory:@(argv[2]) selectionHandler:^(NSDictionary* p){loaded=p;}];
 assert(view.bankCount==62&&view.patchCount==7164);
 NSMenuItem* all=[NSMenuItem new];all.representedObject=@{@"pane":@0,@"bank":@(-1)};[view bankChosen:all];assert([view.filteredPatchCounts[0]unsignedIntegerValue]==7164);
 NSTextField* search=(NSTextField*)view.subviews.firstObject;search.stringValue=@"DreamEcoBC";[view controlTextDidChange:nil];assert([view.filteredPatchCounts[0]unsignedIntegerValue]>0);
 NSEvent* click=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:[view convertPoint:NSMakePoint(30,35) toView:nil] modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:1 clickCount:1 pressure:1];[view mouseDown:click];assert(!loaded);
 NSEvent* dbl=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:[view convertPoint:NSMakePoint(30,35) toView:nil] modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:2 clickCount:2 pressure:1];[view mouseDown:dbl];assert([loaded[@"name"]isEqual:@"DreamEcoBC"]&&[loaded[@"checksumVerified"]boolValue]&&[loaded[@"verifiedValues"]count]>300);
 assert([loaded[@"sysex"]isKindOfClass:NSData.class]&&[loaded[@"sysex"]length]==524);
 search.stringValue=@"no-such-patch-946339";[view controlTextDidChange:nil];assert([view.filteredPatchCounts[0]unsignedIntegerValue]==0);
 search.stringValue=@"";[view controlTextDidChange:nil];[view mouseDown:dbl];NSString* beforeName=loaded[@"name"];NSNumber* beforeProgram=loaded[@"program"];
 BrowserScrollDouble* scroll=[BrowserScrollDouble new];scroll.locationInWindow=[view convertPoint:NSMakePoint(30,35) toView:nil];scroll.scrollingDeltaY=0;
 [view scrollWheel:(NSEvent*)scroll];[view mouseDown:dbl];assert([loaded[@"name"]isEqual:beforeName]&&[loaded[@"program"]isEqual:beforeProgram]);
 scroll.scrollingDeltaY=-1;[view scrollWheel:(NSEvent*)scroll];[view mouseDown:dbl];assert(![loaded[@"program"]isEqual:beforeProgram]);
 NSMenuItem* category=[NSMenuItem new];category.tag=2;[view categoryChosen:category];NSUInteger count=[view.filteredPatchCounts[0]unsignedIntegerValue];assert(count>0&&count<7164);
 category.tag=-1;[view categoryChosen:category];
 NSBitmapImageRep* rep=[[NSBitmapImageRep alloc]initWithBitmapDataPlanes:nil pixelsWide:705 pixelsHigh:426 bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
 NSGraphicsContext* graphics=[NSGraphicsContext graphicsContextWithBitmapImageRep:rep];[view displayRectIgnoringOpacity:view.bounds inContext:graphics];
 [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}]writeToFile:@"native/editor/build/browser-render.png" atomically:YES];
 VTRPatchBrowserView* empty=[[VTRPatchBrowserView alloc]initWithFrame:NSMakeRect(0,0,705,426) resources:@(argv[1]) bankDirectory:@"/nonexistent/virus-test-no-banks" selectionHandler:^(NSDictionary*){}];assert(empty.bankCount==0&&empty.patchCount==0);
 std::cout<<"Browser Cocoa tests passed: bank/search/category filters; single-click selects, double-click emits complete checked dump and mapped values; empty library\n";
}}
