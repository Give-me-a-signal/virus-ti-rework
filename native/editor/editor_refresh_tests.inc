// Real editor heartbeat regressions: idle ticks must not repaint the host's
// entire plug-in window; automation and meters must continue to be visible.
@interface VTRRefreshProbe : VTRNativeEditor {
@public
 NSUInteger fullInvalidations;
 NSMutableArray<NSValue*>* regions;
}
- (void)resetInvalidations;
@end
@implementation VTRRefreshProbe
- (void)setNeedsDisplay:(BOOL)flag {if(flag)++fullInvalidations;[super setNeedsDisplay:flag];}
- (void)setNeedsDisplayInRect:(NSRect)rect {if(!regions)regions=[NSMutableArray array];[regions addObject:[NSValue valueWithRect:rect]];[super setNeedsDisplayInRect:rect];}
- (void)resetInvalidations {fullInvalidations=0;[regions removeAllObjects];[super setNeedsDisplay:NO];}
@end

void checkEditorRefresh() {
 Controller c;assert(c.initialize(nullptr)==kResultOk);
 VTRRefreshProbe* view=[[VTRRefreshProbe alloc]initWithController:&c resources:@"/none"];
 [view refreshEditor];[view resetInvalidations];
 auto revision=c.editorParameterRevision;
 for(int i=0;i<100;++i)[view refreshEditor];
 assert(view->fullInvalidations==0&&view->regions.count==0&&c.editorParameterRevision==revision&&!c.hardwareConnected);
 auto id=[view paramID:@"Filter 1/Cutoff" part:0];
 c.setParamNormalized(id,.5);assert(c.editorParameterRevision==revision+1);
 [view refreshEditor];assert(view->fullInvalidations==1);
 [view resetInvalidations];c.setParamNormalized(id,.5);[view refreshEditor];
 assert(view->fullInvalidations==0&&view->regions.count==0);
 // Readback meters are independent from CC7; do not repaint graph panels.
 for(int theme:{0,1}){view->darkTheme=theme==0;[view applyAppearance];[view resetInvalidations];
  c.hardwareMeters.midiActivity[0]=theme?.25f:.5f;c.hardwareMeters.usbPeak[2]=theme?.1f:.2f;
  [view refreshEditor];assert(view->fullInvalidations==0&&view->regions.count==2);
  for(NSValue* region in view->regions)assert(!NSIntersectsRect(region.rectValue,NSMakeRect(211,49,671,416)));
  [view resetInvalidations];[view refreshEditor];assert(view->regions.count==0&&view->fullInvalidations==0);
 }
 c.hardwareText="Test status update";[view refreshEditor];assert(view->fullInvalidations==0&&view->regions.count==1);
 assert(NSEqualRects(view->regions[0].rectValue,NSMakeRect(258,487,281,22)));
 [view resetInvalidations];++c.patchRevision;[view refreshEditor];assert(view->fullInvalidations>0&&view->displayedPatchRevision==c.patchRevision);
 [view resetInvalidations];for(int i=0;i<10;++i)[view refreshEditor];assert(view->fullInvalidations==0&&view->regions.count==0);
 // Pointer motion within a control updates neither its page nor its tooltip.
 view->selectedPage=3;NSPoint point=NSMakePoint(337,163);NSPoint location=[view convertPoint:point toView:nil];
 NSEvent* event=[NSEvent mouseEventWithType:NSEventTypeMouseMoved location:location modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:0 pressure:0];
 [view mouseMoved:event];assert(view->hovered);[view resetInvalidations];[view mouseMoved:event];assert(view->fullInvalidations==0&&view->regions.count==0);
 [view mouseExited:event];assert(!view->hovered&&view->fullInvalidations==0&&view->regions.count==1);
 // Closing the view always removes its timer; construction did not connect.
 assert(!c.hardwareConnected);view=nil;assert(c.terminate()==kResultOk);
 std::cout<<"Editor refresh: 100 idle ticks without redraw, automation, unchanged values, partial meter/status updates and hover coalescing passed\n";
}
