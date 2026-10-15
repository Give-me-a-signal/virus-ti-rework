#import <Cocoa/Cocoa.h>
#include "editor.mm"
#include <cassert>
#include <iostream>
int main(int argc,char** argv){@autoreleasepool{
 assert(argc==3);[NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
 Controller controller;assert(controller.initialize(nullptr)==kResultOk);
 VTRNativeEditor* view=[[VTRNativeEditor alloc]initWithController:&controller resources:@(argv[1])];assert(view->model[@"project_owned"]);assert(!view->originalUI);assert(view->nodes.count>200);
 [NSFileManager.defaultManager createDirectoryAtPath:@(argv[2]) withIntermediateDirectories:YES attributes:nil error:nil];
 for(int theme=0;theme<2;++theme){view->darkTheme=theme==0;[view applyAppearance];for(int page:{0,1,2,3,4,5,6,7,8,9,11,12}){
  view->selectedPage=page;[view updateBrowserVisibility];for(int zoom:{100,150,200}){[view setEditorSize:NSMakeSize(907*zoom/100.,513*zoom/100.)];
   NSBitmapImageRep* bitmap=[view bitmapImageRepForCachingDisplayInRect:view.bounds];[view cacheDisplayInRect:view.bounds toBitmapImageRep:bitmap];assert(bitmap.pixelsWide>0);
   if(zoom==100){NSString* path=[NSString stringWithFormat:@"%s/%@-%02d.png",argv[2],theme?@"polar":@"dark",page];assert([[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}]writeToFile:path atomically:YES]);}
  }
 }}
 view->selectedPage=3;NSDictionary* cutoff=nil;for(NSDictionary* n in view->nodes)if([n[@"attrs"][@"Parameter"]isEqual:@"Filter 1/Cutoff"]&&[n[@"type"]containsString:@"Knob"])cutoff=n;assert(cutoff&&[view visible:cutoff]&&[view editable:cutoff]);[view applyNode:cutoff plain:63 gesture:YES];assert([view plain:@"Filter 1/Cutoff" part:0]==63);
 view->selectedPage=2;for(NSDictionary* n in view->nodes)if([n[@"attrs"][@"Parameter"]isEqual:@"Oscillator 1/Mode"]){assert(![view editable:n]);}
 assert(!controller.hardwareConnected);assert(!originalAppearanceModel(@"/nonexistent/vtr-appearance",nullptr));
 auto* plug=controller.createView(ViewType::kEditor);assert(plug);ViewRect r{0,0,10,10};assert(plug->checkSizeConstraint(&r)==kResultTrue&&r.getWidth()==907);r={0,0,3000,3000};assert(plug->checkSizeConstraint(&r)==kResultTrue&&r.getWidth()==1814);plug->release();view=nil;controller.terminate();
 if(std::getenv("VTR_TEST_ORIGINAL_APPEARANCE")){auto imported=originalAppearanceModel(originalAppearanceDirectory(),nullptr);assert(imported);Controller c2;assert(c2.initialize(nullptr)==kResultOk);VTRNativeEditor* original=[[VTRNativeEditor alloc]initWithController:&c2 resources:@"/none"];original->originalUI=YES;original->model=imported;original->nodes=imported[@"nodes"];original->parameters=imported[@"parameters"];original->conditions=imported[@"conditions"];[original->nodeByID removeAllObjects];for(NSDictionary* n in original->nodes)original->nodeByID[n[@"id"]]=n;[original applyAppearance];
  for(int page:{0,1,2,3,4,5,6,7,8,9,11,12}){original->selectedPage=page;[original updateBrowserVisibility];NSBitmapImageRep* image=[original bitmapImageRepForCachingDisplayInRect:original.bounds];[original cacheDisplayInRect:original.bounds toBitmapImageRep:image];assert(([[image representationUsingType:NSBitmapImageFileTypePNG properties:@{}]writeToFile:[NSString stringWithFormat:@"%s/original-%02d.png",argv[2],page] atomically:YES]));}original=nil;c2.terminate();}
 std::cout<<"Resource-free editor: both themes, 12 pages, three scales, gestures, disabled controls, resize and missing artwork passed\n";
}return 0;}
