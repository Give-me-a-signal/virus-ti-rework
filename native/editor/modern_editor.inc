// Native vector skin. The recovered node geometry and interaction paths stay authoritative.
- (void)drawModernPanels {
 using namespace Skin;Palette p(darkTheme);box(self.bounds,p.base,nil,0);
 text(@"VIRUS",NSMakeRect(14,8,99,28),25,p.text,NO,NSFontWeightHeavy);
 text(@"TI",NSMakeRect(108,9,35,28),25,p.accent,NO,NSFontWeightLight);
 text(darkTheme?@"D A R K  /  1 6  P A R T S":@"P O L A R  /  1 6  P A R T S",NSMakeRect(15,35,175,10),6.5,p.muted);
 line(NSMakePoint(196,49),NSMakePoint(196,467),p.line,.7);
 for(unsigned part=0;part<16;++part){double y=51+part*26;bool selected=part==selectedPart;box(NSMakeRect(10,y,177,24),selected?p.raised:p.panel,p.line,3);if(selected)box(NSMakeRect(10,y+4,2,16),p.accent,nil,1);}
 if(model[@"project_owned"]){
  box(NSMakeRect(207,49,679,416),p.panel,p.line,5);
  NSArray* titles=@[@"EASY EDIT",@"PATCH BROWSER",@"OSCILLATORS",@"FILTERS & ENVELOPES",@"LFOS",@"MODULATION MATRIX",@"ARPEGGIATOR",@"EFFECTS 1",@"REMOTE",@"COMMON",@"",@"EFFECTS 2",@"CONFIGURATION"];
  text(titles[std::clamp(selectedPage,0L,12L)],NSMakeRect(219,54,640,20),11,p.text,NO,NSFontWeightSemibold);
  text(@"Dimmed controls are unavailable. Graphs are illustrative. Hardware produces the audio.",NSMakeRect(219,444,650,13),8,p.muted);
  if(selectedPage==0)text(@"Connect your TI, choose a part, then open an editor page or import your own patch bank in Browser.",NSMakeRect(230,110,620,80),13,p.text);
  if(selectedPage==8)text(@"Remote template support is not available in this beta.",NSMakeRect(230,110,620,80),13,p.muted);
  if(selectedPage==12)text(@"Native arm64 • AU / VST3 • three stereo returns • RAM recall and restoration\nUse Import Original… to optionally add the original Access appearance.",NSMakeRect(230,110,620,90),13,p.text);
  return;
 }
 auto panel=[&](double x,double y,double w,double h,NSString* title){box(NSMakeRect(x,y,w,h),p.panel,p.line,5);text(title,NSMakeRect(x+10,y+2,w-20,12),8.5,p.muted,NO,NSFontWeightSemibold);line(NSMakePoint(x+10,y+17),NSMakePoint(x+w-10,y+17),p.line,.5);};
 NSInteger page=selectedSection?selectedUtility==1?12:8:selectedPage;
 switch(page){
 case 0:panel(211,49,671,271,@"EASY EDIT");panel(211,328,411,137,@"SOFT KNOBS");panel(629,328,253,137,@"EFFECTS");break;
 case 2:panel(211,49,251,249,@"OSCILLATOR 1");panel(465,49,166,249,@"OSCILLATOR COMMON");panel(634,49,248,249,@"OSCILLATOR 2");panel(211,304,251,78,@"OSCILLATOR 3");panel(211,388,251,77,@"UNISON");panel(468,304,60,78,@"NOISE");panel(468,388,60,77,@"PUNCH");panel(536,304,346,161,@"MIXER");break;
 case 3:panel(211,49,251,249,@"FILTER 1");panel(465,49,166,249,@"FILTER COMMON");panel(634,49,248,249,@"FILTER 2");panel(211,304,251,161,@"FILTER / AUX ENVELOPES");panel(468,304,157,161,@"AMPLIFIER");panel(636,304,246,161,@"AMPLIFIER ENVELOPE");break;
 case 4:for(int i=0;i<3;++i){double h=i==2?112:146;panel(211,49+i*152,i==2?438:357,h,[NSString stringWithFormat:@"LFO %d",i+1]);panel(i==2?657:575,49+i*152,i==2?225:307,h,[NSString stringWithFormat:@"LFO MODULATION %d",i+1]);}break;
 case 5:for(int i=0;i<6;++i){double x=i<3?211:550,y=49+(i%3)*141;panel(x,y,332,135,[NSString stringWithFormat:@"MODULATION SLOT %d",i+1]);text(@"SOURCE",NSMakeRect(x+18,y+24,120,16),7.5,p.muted);for(int row=0;row<3;++row)text([NSString stringWithFormat:@"%d",row+1],NSMakeRect(x+5,y+54+row*24,12,12),7,p.muted); }break;
 case 6:panel(211,49,671,83,@"ARPEGGIATOR");panel(211,139,671,326,@"PATTERN");break;
 case 7:panel(211,49,257,100,@"CHARACTER");panel(211,156,257,136,@"DISTORTION");panel(211,300,257,60,@"INPUT");panel(211,368,257,97,@"ENVELOPE FOLLOWER");panel(475,49,407,100,@"CHORUS");panel(475,156,407,96,@"PHASER");panel(475,261,407,99,@"FILTER BANK");panel(475,368,407,97,@"EQUALIZER");break;
 case 9:panel(211,49,332,83,@"COMMON PARAMETERS");panel(550,49,332,83,@"OUTPUT");panel(211,140,332,82,@"PITCH BENDER");panel(550,140,332,82,@"CATEGORY");panel(211,231,671,84,@"SOFT KNOBS");break;
 case 11:panel(211,49,671,135,@"DELAY");panel(211,191,671,134,@"REVERB");panel(211,332,671,133,@"VOCODER");break;
 case 8:panel(211,49,671,416,@"REMOTE");break;
 case 12:panel(211,49,671,416,@"CONFIGURATION");break;
 }
}
- (void)drawModernNode:(NSDictionary*)node {
 using namespace Skin;if(![self visible:node])return;Palette p(darkTheme);
 NSDictionary* a=node[@"attrs"];NSString* type=node[@"type"],*name=a[@"Parameter"]?:@"";NSRect r=rectOf(node[@"absolute_rect"]);
 if(r.size.width<=0||r.size.height<=0||NSMinY(r)>=471)return;
 if([type isEqual:@"Container"]||[type isEqual:@"Frame"]||[type isEqual:@"PatchContainer"]||[name hasPrefix:@"LockView/"]||[name hasPrefix:@"Locks/"]||[type isEqual:@"PatchSaveMenu"])return;
 if([name hasPrefix:@"Repository Actions/"])return;
 NSInteger part=[self partOf:node];double v=[self plain:name part:part];NSDictionary* def=[self definition:name];double lo=[def[@"minValue"]doubleValue],hi=[def[@"maxValue"]doubleValue];double n=hi>lo?std::clamp((v-lo)/(hi-lo),0.,1.):0;
 bool editable=[self editable:node];NSColor* fg=editable?p.text:p.muted;
 if([name isEqual:@"VirusControl/SelectedPage"]||[name isEqual:@"VirusControl/SelectedUtilityPage"]){
  NSArray* pages=@[@"EASY",@"BROWSER",@"OSC",@"FILTER",@"LFO",@"MATRIX",@"ARP",@"FX 1",@"REMOTE",@"COMMON",@"",@"FX 2",@"CONFIG"];
  NSInteger index=[a[@"OnValue"]integerValue];NSString* title=[name hasSuffix:@"SelectedUtilityPage"]?(index?@"CONFIG":@"REMOTE"):pages[std::clamp(index,0L,12L)];
  BOOL active=v==index;box(NSInsetRect(r,1,1),active?p.raised:p.base,active?p.line:nil,3);text(title,r,8.5,active?p.text:p.muted,YES,NSFontWeightSemibold);if(active)box(NSMakeRect(r.origin.x+10,NSMaxY(r)-2,r.size.width-20,1.5),p.accent,nil,0);return;
 }
 if([name isEqual:@"VirusControl/Selected Section"]){text(selectedSection?@"PATCH":@"UTILITY",r,7.5,p.muted,YES,NSFontWeightSemibold);return;}
 if([name isEqual:@"Multi Parameters/Panel Part"]){text([NSString stringWithFormat:@"%02ld",(long)[a[@"OnValue"]integerValue]+1],r,10,v==[a[@"OnValue"]doubleValue]?p.accent:p.muted,YES,NSFontWeightSemibold);return;}
 if([type isEqual:@"NameEdit"]){r.size.width=107;text(patchNames[@(part)]?:@"Init / Offline",r,8.5,p.text,NO,NSFontWeightMedium);return;}
 if([type isEqual:@"VUSlider"]){double activity=controller->hardwareMeters.midiActivity[std::clamp(part,0L,15L)];NSRect track=NSMakeRect(r.origin.x+4,r.origin.y+4,r.size.width-8,4);box(track,p.line,nil,1);box(NSMakeRect(track.origin.x,track.origin.y,track.size.width*activity,track.size.height),p.accent,nil,1);double x=track.origin.x+n*track.size.width;box(NSMakeRect(x-1,r.origin.y+1,2,10),p.text,nil,.5);return;}
 if(Graphs::supports(type)){
  box(r,p.display,p.line,3);[NSGraphicsContext saveGraphicsState];[[NSBezierPath bezierPathWithRect:r]addClip];for(int i=1;i<4;i++){line(NSMakePoint(r.origin.x,r.origin.y+r.size.height*i/4),NSMakePoint(NSMaxX(r),r.origin.y+r.size.height*i/4),p.line,.35);}
  auto g=Graphs::geometry(type,a,r,^double(NSString* parameter){return [self plain:parameter part:part];});
  for(auto& points:g.paths){auto path=[NSBezierPath bezierPath];bool first=true;for(auto pt:points){if(first)[path moveToPoint:pt];else[path lineToPoint:pt];first=false;}[p.accent setStroke];path.lineWidth=1.1;[path stroke];}
  for(auto& h:g.handles)box(NSMakeRect(h.point.x-2,h.point.y-2,4,4),p.ink,p.accent,1);[NSGraphicsContext restoreGraphicsState];return;
 }
 if([type isEqual:@"VerticalKnob"]||[type isEqual:@"HorizontalKnob"]||[type isEqual:@"InverseHorizontalKnob"]){
  if([name hasSuffix:@"/Mode"]){NSArray* modes=@[@"LP",@"HP",@"BP",@"BS",@"AN 1",@"AN 2",@"AN 3",@"AN 4"];text(modes[std::clamp(NSInteger(v),0L,7L)],r,7,p.ink,YES,NSFontWeightSemibold);return;}
  box(r,p.display,p.line,3);
  if([name hasSuffix:@"/WaveSelect"]||[name hasPrefix:@"LFO"]){
   NSBezierPath* wave=[NSBezierPath bezierPath];for(int i=0;i<64;++i){double x=i/63.,y=0;int shape=int(v);
    if([name hasSuffix:@"/WaveSelect"]||shape>=6){float sample;auto bits=Graphs::waveformBits[std::clamp([name hasPrefix:@"LFO"]?shape-4:shape,0,63)][i];std::memcpy(&sample,&bits,4);y=sample;}
    else if(shape==0)y=sin(x*2*M_PI);else if(shape==1)y=1-4*std::abs(x-.5);else if(shape==2)y=1-2*x;else if(shape==3)y=x<.5?1:-1;else if(shape==4)y=sin(std::floor(x*8)*2.37);else {double a=std::floor(x*8),t=x*8-a;y=sin(a*2.37)*(1-t)+sin((a+1)*2.37)*t;}
    NSPoint pt=NSMakePoint(r.origin.x+3+x*(r.size.width-6),NSMidY(r)-y*(r.size.height-6)*.45);if(i)[wave lineToPoint:pt];else[wave moveToPoint:pt];}[p.accent setStroke];wave.lineWidth=.9;[wave stroke];
  }else{text([NSString stringWithFormat:@"%@  %@",label(name),[self textFor:node value:v]],NSInsetRect(r,3,0),8,p.ink,YES);}
  return;
 }
 if([type containsString:@"Knob"]){
  bool hollow=[type isEqual:@"MaskedKnob"];knob(r,n,p,hollow,editable);
  if(r.origin.x>200&&(!hollow||selectedPage==0||selectedPage==4||selectedPage==11)){text(model[@"project_owned"]?[name stringByReplacingOccurrencesOfString:@"/" withString:@"\n"]:label(name),NSMakeRect(r.origin.x-20,NSMaxY(r)+1,r.size.width+40,22),6.5,p.muted,YES);}
  return;
 }
 if([type isEqual:@"HorizontalSlider"]||[type isEqual:@"VerticalSlider"]){
  bool horizontal=[type isEqual:@"HorizontalSlider"];NSRect track=horizontal?NSMakeRect(r.origin.x+4,NSMidY(r)-2,r.size.width-8,4):NSMakeRect(NSMidX(r)-2,r.origin.y+4,4,r.size.height-8);box(track,p.line,nil,2);
  double x=horizontal?track.origin.x+n*track.size.width:NSMidX(r),y=horizontal?NSMidY(r):NSMaxY(track)-n*track.size.height;box(NSMakeRect(x-3,y-5,6,10),p.raised,p.muted,2);
  if(selectedPage!=5&&r.origin.x>200)text(label(name),NSMakeRect(r.origin.x,NSMaxY(r)+1,r.size.width,10),7.5,p.muted,YES);return;
 }
 if([type containsString:@"Menu"]||[type isEqual:@"TextSlider"]){
  box(NSInsetRect(r,.5,.5),p.display,p.line,3);bool menu=[type containsString:@"Menu"];NSRect tr=NSInsetRect(r,menu?5:2,0);if(menu)tr.size.width-=9;text([self textFor:node value:v],tr,r.size.height>18?9:8,p.ink,YES,NSFontWeightMedium);
  if(selectedPage==9||model[@"project_owned"]){NSString* caption=label(name);if([name hasPrefix:@"Soft Knobs/"])caption=[name.lastPathComponent containsString:@"Name"]?@"NAME":@"DESTINATION";if(![name hasPrefix:@"Soft Knobs/"])text(caption,NSMakeRect(r.origin.x,r.origin.y-12,r.size.width,10),7.5,p.muted,YES);}
  if(menu){double x=NSMaxX(r)-7,y=NSMidY(r);line(NSMakePoint(x-2,y-1),NSMakePoint(x,y+1),p.muted,.8);line(NSMakePoint(x,y+1),NSMakePoint(x+2,y-1),p.muted,.8);}return;
 }
 if([type containsString:@"Button"]){
  BOOL radio=[type isEqual:@"RadioButton"],active=radio?v==[a[@"OnValue"]doubleValue]:v!=lo;
  NSString* title=radio?[self textFor:node value:[a[@"OnValue"]doubleValue]]:label(name);
  if([name hasPrefix:@"Filter"]&&[name hasSuffix:@"/Mode"]&&radio)title=@[@"LP",@"HP",@"BP",@"BS",@"AN 1",@"AN 2",@"AN 3",@"AN 4"][std::clamp([a[@"OnValue"]integerValue],0L,7L)];
  if([name isEqual:@"Filters/Routing"]&&radio)title=@[@"SER 4",@"SER 6",@"PAR",@"SPLIT"][std::clamp([a[@"OnValue"]integerValue],0L,3L)];
  if([name containsString:@"Stages"]&&radio)title=[NSString stringWithFormat:@"%ld",(long)[a[@"OnValue"]integerValue]+1];
  if([name containsString:@"Link"]&&r.size.width<35)title=@"↔";
  if(r.origin.x<100&&![name isEqual:@"Multi Parameters/Panel Part"])title=[a[@"NormalBack"]containsString:@"Mute"]?@"M":@"D";
  if([name containsString:@"SelectedEnvelope"]&&radio)title=[NSString stringWithFormat:@"%ld",(long)[a[@"OnValue"]integerValue]];
  if([type isEqual:@"IncButton"]||[type isEqual:@"DecButton"])title=[type isEqual:@"IncButton"]?@"+":@"−";
  if(r.size.width<20&&title.length>3){if(![type isEqual:@"IncButton"]&&![type isEqual:@"DecButton"])title=@"";}
  box(NSInsetRect(r,.7,.7),active?p.raised:p.panel,active?p.accent:p.line,2);text(title,NSInsetRect(r,1,0),r.size.height<15?7:8,active?p.accent:fg,YES,NSFontWeightMedium);
  if(!title.length){double side=std::min(5.,r.size.height*.3);box(NSMakeRect(NSMidX(r)-side/2,NSMidY(r)-side/2,side,side),active?p.accent:p.muted,nil,side/2);}return;
 }
 if([type isEqual:@"StaticText"]){text([self textFor:node value:v],r,8,p.muted,YES);return;}
 if([type isEqual:@"ArpEdit"]||[type isEqual:@"RemoteBrowser"]){box(r,p.display,p.line,3);text([type isEqual:@"ArpEdit"]?@"Pattern editing is not available yet":@"No remote template loaded",NSInsetRect(r,16,16),10,p.muted);}
}
- (void)applyAppearance {
 self.appearance=[NSAppearance appearanceNamed:(originalUI||darkTheme)?NSAppearanceNameDarkAqua:NSAppearanceNameAqua];
 themeButton.title=darkTheme?@"◐  Dark":@"◑  Polar";themeButton.enabled=!originalUI;[uiStyleButton selectItemWithTag:originalUI?1:0];[browser setOriginalAppearance:originalUI];[browser setDarkTheme:darkTheme];[self setNeedsDisplay:YES];
}
- (void)changeUIStyle:(NSPopUpButton*)sender {
 [self finishGraphGesture];if(dragID!=kNoParamId)controller->endEdit(dragID);dragID=kNoParamId;dragging=nil;focused=nil;hovered=nil;
 NSInteger choice=sender.selectedTag;
 if(choice==2){chooseOriginalAppearance();choice=0;}
 NSError* error=nil;NSDictionary* imported=choice==1?originalAppearanceModel(originalAppearanceDirectory(),&error):nil;
 if(choice==1&&!imported){if(chooseOriginalAppearance())imported=originalAppearanceModel(originalAppearanceDirectory(),&error);}
 originalUI=imported!=nil;model=imported?:builtinEditorModel();nodes=model[@"nodes"];parameters=model[@"parameters"];conditions=model[@"conditions"];
 [images removeAllObjects];[nodeByID removeAllObjects];for(NSDictionary* n in nodes)nodeByID[n[@"id"]]=n;
 [[[NSUserDefaults alloc]initWithSuiteName:@"org.virus-ti-rework.editor"]setObject:originalUI?@"original":darkTheme?@"dark":@"polar" forKey:@"appearance"];
 [self applyAppearance];
}
- (void)changeTheme:(id)sender {
 if(originalUI)return;
 darkTheme=!darkTheme;NSUserDefaults* prefs=[[NSUserDefaults alloc]initWithSuiteName:@"org.virus-ti-rework.editor"];[prefs setBool:!darkTheme forKey:@"polarTheme"];[self applyAppearance];
}
- (void)changeScale:(NSPopUpButton*)sender {if(resizeHandler)resizeHandler(sender.selectedTag/100.);}
- (void)setEditorSize:(NSSize)size {
 [self setFrameSize:size];[self setBoundsSize:NSMakeSize(907,513)];[self setNeedsDisplay:YES];
 NSInteger percent=std::lround(size.width/907.*100);[scaleButton selectItemWithTag:percent];
}
