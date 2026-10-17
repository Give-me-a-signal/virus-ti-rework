// Structural regression checks for the user-visible classic workflow. These
// assert grouping/relationships, not a node-count or successful bitmap allocation.
void checkClassicLayout(VTRNativeEditor* view,Controller& controller) {
 assert([view->model[@"classic_layout"]boolValue]);
 for(const auto& spec:kControls){NSDictionary* d=[view definition:@(spec.name)];assert([d[@"minValue"]doubleValue]==spec.minimum&&[d[@"maxValue"]doubleValue]==spec.maximum&&[d[@"defaultValue"]doubleValue]==spec.initial);}
 auto find=[&](NSString* name,NSString* type,int page)->NSDictionary*{for(NSDictionary* n in view->nodes)if([n[@"page"]intValue]==page&&[n[@"attrs"][@"Parameter"]isEqual:name]&&[n[@"type"]isEqual:type]&&[view visible:n])return n;assert(false&&"Required classic control missing");return nil;};
 std::vector<double> before;for(const auto& s:kCatalog)before.push_back(controller.getParamNormalized(s.id));
 view->selectedPage=2;
 auto osc1=find(@"Oscillator 1/Shape",@"MaskedKnob",2),osc2=find(@"Oscillator 2/Shape",@"MaskedKnob",2);
 NSRect a=rectOf(osc1[@"absolute_rect"]),b=rectOf(osc2[@"absolute_rect"]);
 assert(NSContainsRect(NSMakeRect(211,49,251,249),a)&&NSContainsRect(NSMakeRect(634,49,248,249),b));
 assert(NSMidY(a)==NSMidY(b)&&a.size.width>=100&&b.size.width==a.size.width);
 NSRect detune=rectOf(find(@"Oscillator 2/Detune",@"Knob",2)[@"absolute_rect"]);
 assert(NSMinX(detune)>NSMaxX(b)&&NSMinY(detune)>49&&NSMaxX(detune)<882);
 for(int osc=1;osc<=2;++osc){NSString* prefix=[NSString stringWithFormat:@"Oscillator %d",osc];NSRect ringRect=rectOf(find([prefix stringByAppendingString:@"/Shape"],@"MaskedKnob",2)[@"absolute_rect"]),graphRect=rectOf(find([prefix stringByAppendingString:@"/PulseWidth"],@"OscClassicShape",2)[@"absolute_rect"]);assert(NSContainsRect(ringRect,graphRect)&&std::abs(NSMidX(ringRect)-NSMidX(graphRect))<=2);}
 assert(NSContainsRect(NSMakeRect(536,304,346,161),rectOf(find(@"Suboscillator/Volume",@"HorizontalSlider",2)[@"absolute_rect"])));
 // Actual preset readback can choose a model, without making its unsupported
 // selector editable or writing a newly invented host/device parameter.
 for(int osc=1;osc<=2;++osc){NSString* mode=[NSString stringWithFormat:@"Oscillator %d/Mode",osc];auto selector=find(mode,@"OptionMenu",2);assert(![view editable:selector]);
  for(int value=0;value<8;++value){[view writeParameter:mode part:0 value:value gesture:NO];int ringCount=0,graphCount=0;for(NSDictionary* n in view->nodes)if([n[@"page"]intValue]==2&&[n[@"attrs"][@"Parameter"]hasPrefix:[NSString stringWithFormat:@"Oscillator %d",osc]]&&[view visible:n]){if([n[@"type"]isEqual:@"MaskedKnob"])++ringCount;if(Graphs::supports(n[@"type"])||[n[@"type"]isEqual:@"HorizontalKnob"])++graphCount;}assert(ringCount==1&&graphCount==1);}
  [view writeParameter:mode part:0 value:0 gesture:NO];
 }
 view->selectedPage=3;
 for(int f=1;f<=2;++f){NSString* mode=[NSString stringWithFormat:@"Filter %d/Mode",f];
  for(int value=0;value<8;++value){[view writeParameter:mode part:0 value:value gesture:NO];int graphs=0;for(NSDictionary* n in view->nodes)if([n[@"page"]intValue]==3&&[n[@"attrs"][@"Parameter"]isEqual:[NSString stringWithFormat:@"Filter %d/Cutoff",f]]&&Graphs::supports(n[@"type"])&&[view visible:n])++graphs;assert(graphs==1);}
  [view writeParameter:mode part:0 value:0 gesture:NO];
 }
 // Utility switches to Remote/Configuration and returns to the same patch page.
 view->selectedPage=1;[view updateBrowserVisibility];assert(!view->browser.hidden);
 auto utility=find(@"VirusControl/Selected Section",@"Button",-1);
 [view applyNode:utility plain:1 gesture:YES];assert(view->selectedSection==1&&view->browser.hidden);
 NSDictionary* configuration=nil;for(NSDictionary* n in view->nodes)if([n[@"attrs"][@"Parameter"]isEqual:@"VirusControl/SelectedUtilityPage"]&&[n[@"attrs"][@"OnValue"]intValue]==1&&[view visible:n])configuration=n;assert(configuration);
 [view applyNode:configuration plain:1 gesture:YES];assert(view->selectedUtility==1&&view->selectedPage==1);
 [view applyNode:utility plain:0 gesture:YES];assert(view->selectedSection==0&&view->selectedPage==1&&!view->browser.hidden);

 // Two-column/six-slot matrix, rather than a registry-order grid.
 view->selectedPage=5;
 for(int slot=1;slot<=6;++slot){NSRect source=rectOf(find([NSString stringWithFormat:@"Matrix %d/Source",slot],@"OptionMenu",5)[@"absolute_rect"]);NSRect panel=NSMakeRect(slot<=3?211:550,49+((slot-1)%3)*141,332,135);assert(NSContainsRect(panel,source));for(int d=1;d<=3;++d){auto amount=find([NSString stringWithFormat:@"Matrix %d/Amount %d",slot,d],@"HorizontalSlider",5);auto target=find([NSString stringWithFormat:@"Matrix %d/Destination %d",slot,d],@"OptionMenu",5);assert(NSMaxX(rectOf(amount[@"absolute_rect"]))<NSMinX(rectOf(target[@"absolute_rect"])));assert(![view editable:target]);}}
 // Both palettes keep identical geometry and all real hit targets at all scales.
 for(int theme:{0,1}){view->darkTheme=theme==0;[view applyAppearance];for(int zoom:{100,125,150,175,200}){[view setEditorSize:NSMakeSize(907*zoom/100.,513*zoom/100.)];
  for(int page:{0,1,2,3,4,5,6,7,8,9,11,12}){view->selectedPage=page;view->selectedSection=page==8||page==12;view->selectedUtility=page==12;std::vector<NSDictionary*> targets;
   for(NSDictionary* n in view->nodes)if([view visible:n]){NSRect r=rectOf(n[@"absolute_rect"]);assert(NSContainsRect(NSMakeRect(0,0,907,471),r));
    if([view editable:n]){NSPoint p=NSMakePoint(NSMidX(r),NSMidY(r));assert([view nodeAt:p]==n);NSPoint physical=[view convertPoint:p toView:nil],back=[view convertPoint:physical fromView:nil];assert(std::abs(p.x-back.x)<.001&&std::abs(p.y-back.y)<.001);targets.push_back(n);}
   }
   for(size_t i=0;i<targets.size();++i)for(size_t j=i+1;j<targets.size();++j)assert(!NSIntersectsRect(rectOf(targets[i][@"absolute_rect"]),rectOf(targets[j][@"absolute_rect"])));
  }
 }}
 size_t i=0;for(const auto& s:kCatalog)assert(controller.getParamNormalized(s.id)==before[i++]);
 assert(!controller.hardwareConnected);view->darkTheme=YES;view->selectedPage=0;view->selectedSection=0;view->selectedUtility=0;[view setEditorSize:NSMakeSize(907,513)];[view applyAppearance];
}
