#import "browser.h"
#include "browser_bank.hpp"
#include "bitmap_layout.h"
#include "modern_theme.h"
#include <algorithm>
#include <memory>
#include <vector>
#include <string>
namespace {
struct LibraryBank {std::string title;virus::patches::Bank bank;};
struct Ref {std::size_t bank,patch;};
NSRect paneRect(NSInteger pane){return NSMakeRect(19,pane?243:29,671,178);}
}
@interface VTRPatchBrowserView () <NSTextFieldDelegate> {
 NSString* resourceDirectory;
 NSMutableDictionary* imageCache;
 VTRPatchSelectionHandler onSelection;
 NSTextField* search;
 std::vector<LibraryBank> library;
 std::vector<Ref> filtered[2];
 NSInteger chosenBank[2],chosenCategory,activePane,selected[2];
 NSUInteger offset[2],totalPatches,rejectedBanks;
 NSString* status;
 BOOL darkTheme;
 BOOL originalAppearance;
}
@end
@implementation VTRPatchBrowserView
- (BOOL)isFlipped{return YES;}
- (BOOL)acceptsFirstMouse:(NSEvent*)event {(void)event;return YES;}
- (BOOL)acceptsFirstResponder{return YES;}
- (instancetype)initWithFrame:(NSRect)frame resources:(NSString*)resources bankDirectory:(NSString*)directory selectionHandler:(VTRPatchSelectionHandler)handler {
 if((self=[super initWithFrame:frame])){
  resourceDirectory=[resources copy];imageCache=[NSMutableDictionary dictionary];onSelection=[handler copy];chosenBank[0]=0;chosenBank[1]=1;chosenCategory=-1;activePane=0;selected[0]=selected[1]=-1;
  NSFileManager* fm=NSFileManager.defaultManager;
  NSArray* paths=[[[fm enumeratorAtPath:directory]allObjects]sortedArrayUsingSelector:@selector(localizedStandardCompare:)];
  for(NSString* relative in paths){if(![@[@"mid",@"syx"]containsObject:relative.pathExtension.lowercaseString])continue;
   NSString* path=[directory stringByAppendingPathComponent:relative];NSDictionary* attr=[fm attributesOfItemAtPath:path error:nil];
   if(![attr[NSFileType]isEqual:NSFileTypeRegular]||[attr[NSFileSize]unsignedLongLongValue]>16*1024*1024){++rejectedBanks;continue;}
   NSData* data=[NSData dataWithContentsOfFile:path options:NSDataReadingMappedIfSafe error:nil];if(!data){++rejectedBanks;continue;}
   try{auto bank=[relative.pathExtension.lowercaseString isEqual:@"mid"]?virus::patches::parse_midi_bank((const std::uint8_t*)data.bytes,data.length):virus::patches::parse_sysex_bank((const std::uint8_t*)data.bytes,data.length);totalPatches+=bank.patches.size();library.push_back({relative.stringByDeletingPathExtension.UTF8String,std::move(bank)});}catch(const virus::patches::ParseError&){++rejectedBanks;}
  }
  // Prefer two actual populated libraries initially; the shipped "My Patches"
  // empty template remains selectable and is represented honestly as empty.
  chosenBank[0]=chosenBank[1]=0;NSInteger populated=0;
  for(std::size_t n=0;n<library.size()&&populated<2;++n)if(!library[n].bank.patches.empty())chosenBank[populated++]=n;
  if(populated<2)chosenBank[1]=chosenBank[0];
  search=[[NSTextField alloc]initWithFrame:NSMakeRect(620,8,65,17)];search.font=[NSFont monospacedSystemFontOfSize:10 weight:NSFontWeightRegular];search.textColor=NSColor.whiteColor;search.backgroundColor=[NSColor colorWithWhite:0.15 alpha:1];search.bordered=NO;search.focusRingType=NSFocusRingTypeNone;search.placeholderString=@"Search";search.delegate=self;search.toolTip=@"Search patch names in the selected banks";[self addSubview:search];
  [self rebuild];
 }
 return self;
}
- (void)setHardwareBuffers:(NSArray<NSData*>*)buffers {
 virus::patches::Bank bank;for(NSData* d in buffers)try{auto b=(const uint8_t*)d.bytes;bank.patches.push_back(virus::patches::parse_single(std::vector<uint8_t>(b,b+d.length)));}catch(...){}
 auto found=std::find_if(library.begin(),library.end(),[](auto& b){return b.title=="Hardware edit buffers";});
 if(found!=library.end()){totalPatches-=found->bank.patches.size();found->bank=std::move(bank);totalPatches+=found->bank.patches.size();}
 else if(!bank.patches.empty()){totalPatches+=bank.patches.size();library.insert(library.begin(),{"Hardware edit buffers",std::move(bank)});}
 [self rebuild];
}
- (NSUInteger)bankCount{return library.size();}
- (NSUInteger)patchCount{return totalPatches;}
- (NSArray<NSNumber*>*)filteredPatchCounts{return @[@(filtered[0].size()),@(filtered[1].size())];}
- (NSString*)statusText{return status?:@"No local patch banks";}
- (NSImage*)image:(NSString*)name {NSImage* im=imageCache[name];if(!im){im=VirusRework::Artwork::load([resourceDirectory stringByAppendingPathComponent:name]);if(im)imageCache[name]=im;}return im;}
- (void)image:(NSString*)name at:(NSRect)rect {
 NSImage* im=[self image:name];if(!im)return;
 if([name isEqual:@"DSP_Menu_2.png"]){VirusRework::Artwork::draw(im,rect,NSMakeRect(0,0,im.size.width,im.size.height),NSEdgeInsetsMake(0,10,0,22));return;}
 [im drawInRect:rect fromRect:NSMakeRect(0,im.size.height-rect.size.height,rect.size.width,rect.size.height) operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];
}
- (void)setOriginalAppearance:(BOOL)original {originalAppearance=original;[self setDarkTheme:darkTheme];}
- (void)setDarkTheme:(BOOL)dark {darkTheme=dark;if(originalAppearance){search.backgroundColor=[NSColor colorWithWhite:.15 alpha:1];search.textColor=NSColor.whiteColor;search.font=[NSFont monospacedSystemFontOfSize:10 weight:NSFontWeightRegular];self.appearance=[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];[self setNeedsDisplay:YES];return;}VirusRework::Skin::Palette p(dark);search.backgroundColor=p.display;search.textColor=p.ink;search.font=[NSFont systemFontOfSize:9];self.appearance=[NSAppearance appearanceNamed:dark?NSAppearanceNameDarkAqua:NSAppearanceNameAqua];[self setNeedsDisplay:YES];}
- (void)text:(NSString*)text at:(NSPoint)p width:(CGFloat)width selected:(BOOL)isSelected {
 if(originalAppearance){
 NSImage* atlas=[self image:isSelected?@"FNT_LCD_5x7_Rot_Browser.png":@"FNT_LCD_5x7_White.png"];
 if(!atlas)return;const CGFloat w=6,h=9;
 for(NSUInteger n=0;n<text.length&&n*w+w<=width;++n){unichar ch=[text characterAtIndex:n];if(ch<32||ch>127)ch='?';
  [atlas drawInRect:NSMakeRect(p.x+n*w,p.y,w,h) fromRect:NSMakeRect(0,atlas.size.height-(ch-31)*h,w,h) operation:NSCompositingOperationSourceOver fraction:isSelected?1:.76 respectFlipped:YES hints:nil];
 }
 return;}
 VirusRework::Skin::Palette palette(darkTheme);VirusRework::Skin::text(text,NSMakeRect(p.x,p.y,width,10),8,isSelected?palette.accent:palette.text);
}
- (void)rebuild {
 NSString* query=[search.stringValue stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
 for(int pane=0;pane<2;++pane){filtered[pane].clear();offset[pane]=0;selected[pane]=-1;
  for(std::size_t b=0;b<library.size();++b){if(chosenBank[pane]>=0&&b!=std::size_t(chosenBank[pane]))continue;
   for(std::size_t p=0;p<library[b].bank.patches.size();++p){const auto& patch=library[b].bank.patches[p];
    if(chosenCategory>=0&&patch.category1!=chosenCategory&&patch.category2!=chosenCategory)continue;
    NSString* name=@(patch.name.c_str());if(query.length&&[name rangeOfString:query options:NSCaseInsensitiveSearch|NSDiacriticInsensitiveSearch].location==NSNotFound)continue;
    filtered[pane].push_back({b,p});
   }
  }
 }
 status=[NSString stringWithFormat:@"%lu local patches | double-click to recall%@",(unsigned long)totalPatches,rejectedBanks?[NSString stringWithFormat:@" | %lu unreadable banks",(unsigned long)rejectedBanks]:@""];
 [self setNeedsDisplay:YES];
}
- (void)controlTextDidChange:(NSNotification*)note {(void)note;[self rebuild];}
- (void)drawOriginalBrowser {
 [self image:@"BG_Page_Browser.png" at:NSMakeRect(0,0,705,426)];
 for(int pane=0;pane<2;++pane){const NSRect r=paneRect(pane);[[NSColor colorWithWhite:0.115 alpha:1]setFill];NSRectFill(NSInsetRect(r,1,1));
  [self image:activePane==pane?@"DSP_BG_Browser_Focus.png":@"DSP_BG_Browser_DeFocus.png" at:r];
  if(filtered[pane].empty())[self text:library.empty()?@"No local patch banks":@"No patches match this bank and filter" at:NSMakePoint(r.origin.x+8,r.origin.y+8) width:640 selected:NO];
  for(NSUInteger i=0;i<128&&offset[pane]+i<filtered[pane].size();++i){const auto ref=filtered[pane][offset[pane]+i];const auto& patch=library[ref.bank].bank.patches[ref.patch];
   NSRect cell=NSMakeRect(r.origin.x+1+(i/16)*83.6,r.origin.y+1+(i%16)*11,83.6,11);BOOL sel=selected[pane]==NSInteger(offset[pane]+i);
   if(sel){[[NSColor colorWithRed:35./255 green:57./255 blue:92./255 alpha:1]setFill];NSRectFill(cell);}
   [self text:[NSString stringWithFormat:@"%3u",patch.program] at:NSMakePoint(cell.origin.x,cell.origin.y+1) width:18 selected:sel];
   [self text:@(patch.name.c_str()) at:NSMakePoint(cell.origin.x+21,cell.origin.y+1) width:60 selected:sel];
  }
  CGFloat y=pane?219:5;[self image:@"DSP_Menu_2.png" at:NSMakeRect(20,y,456,23)];
  NSString* title=chosenBank[pane]<0?@"All Local Banks":(library.empty()?@"No local banks":@(library[std::min(std::size_t(chosenBank[pane]),library.size()-1)].title.c_str()));
  [self text:title at:NSMakePoint(26,y+7) width:442 selected:NO];
  [self image:@"DSP_Menu_Inc_2.png" at:NSMakeRect(478,y+7,17,9)];[self image:@"DSP_Menu_Dec_2.png" at:NSMakeRect(499,y+8,17,9)];
 }
 [self image:chosenCategory<0?@"BTN_Category_OFF.png":@"BTN_Category_NRM.png" at:NSMakeRect(530,9,53,15)];
 [self image:@"DSP_BG_Mag_Glass.png" at:NSMakeRect(599,6,21,21)];
 [[NSColor colorWithRed:35./255 green:57./255 blue:92./255 alpha:1]setFill];NSRectFill(NSMakeRect(20,207,669,11));
 [self text:self.statusText at:NSMakePoint(21,208) width:667 selected:YES];
}
- (void)drawRect:(NSRect)dirty {(void)dirty;
 if(originalAppearance){[self drawOriginalBrowser];return;}
 using namespace VirusRework::Skin;Palette palette(darkTheme);box(self.bounds,palette.base,nil,0);
 for(int pane=0;pane<2;++pane){const NSRect r=paneRect(pane);box(r,palette.panel,palette.line,4);
  if(activePane==pane)line(NSMakePoint(r.origin.x+4,r.origin.y),NSMakePoint(NSMaxX(r)-4,r.origin.y),palette.accent,1);
  if(filtered[pane].empty())[self text:library.empty()?@"No local patch banks":@"No patches match this bank and filter" at:NSMakePoint(r.origin.x+8,r.origin.y+8) width:640 selected:NO];
  for(NSUInteger i=0;i<128&&offset[pane]+i<filtered[pane].size();++i){const auto ref=filtered[pane][offset[pane]+i];const auto& patch=library[ref.bank].bank.patches[ref.patch];
   NSRect cell=NSMakeRect(r.origin.x+1+(i/16)*83.6,r.origin.y+1+(i%16)*11,83.6,11);BOOL sel=selected[pane]==NSInteger(offset[pane]+i);
   if(sel){box(cell,palette.raised,nil,1);}
   [self text:[NSString stringWithFormat:@"%3u",patch.program] at:NSMakePoint(cell.origin.x,cell.origin.y+1) width:18 selected:sel];
   [self text:@(patch.name.c_str()) at:NSMakePoint(cell.origin.x+21,cell.origin.y+1) width:60 selected:sel];
  }
  CGFloat y=pane?219:5;box(NSMakeRect(20,y,456,23),palette.display,palette.line,3);text(@"⌄",NSMakeRect(458,y,14,23),10,palette.muted,YES);
  NSString* title=chosenBank[pane]<0?@"All Local Banks":(library.empty()?@"No local banks":@(library[std::min(std::size_t(chosenBank[pane]),library.size()-1)].title.c_str()));
  [self text:title at:NSMakePoint(26,y+7) width:442 selected:NO];
  text(@"‹",NSMakeRect(478,y,17,23),14,palette.text,YES);text(@"›",NSMakeRect(499,y,17,23),14,palette.text,YES);
 }
 box(NSMakeRect(529,5,57,22),palette.panel,palette.line,3);text(@"Category",NSMakeRect(529,5,57,22),8,chosenCategory<0?palette.muted:palette.accent,YES);
 text(@"⌕",NSMakeRect(599,6,21,21),14,palette.muted,YES);
 text(self.statusText,NSMakeRect(20,207,669,11),7.5,palette.muted);

}
- (void)showBanks:(NSInteger)pane event:(NSEvent*)event {
 NSMenu* menu=[[NSMenu alloc]initWithTitle:@"Local Patch Banks"];
 NSMenuItem* all=[[NSMenuItem alloc]initWithTitle:@"All Local Banks" action:@selector(bankChosen:) keyEquivalent:@""];all.target=self;all.representedObject=@{@"pane":@(pane),@"bank":@(-1)};all.state=chosenBank[pane]<0?NSControlStateValueOn:NSControlStateValueOff;[menu addItem:all];[menu addItem:NSMenuItem.separatorItem];
 for(std::size_t n=0;n<library.size();++n){NSMenuItem* item=[[NSMenuItem alloc]initWithTitle:@(library[n].title.c_str()) action:@selector(bankChosen:) keyEquivalent:@""];item.target=self;item.representedObject=@{@"pane":@(pane),@"bank":@(n)};item.state=chosenBank[pane]==NSInteger(n)?NSControlStateValueOn:NSControlStateValueOff;[menu addItem:item];}
 [menu addItem:NSMenuItem.separatorItem];NSMenuItem* import=[[NSMenuItem alloc]initWithTitle:@"Open bank or preset…" action:@selector(importBank:) keyEquivalent:@""];import.target=self;import.tag=pane;[menu addItem:import];
 [NSMenu popUpContextMenu:menu withEvent:event forView:self];
}
- (void)importBank:(NSMenuItem*)item {
 NSOpenPanel* panel=[NSOpenPanel openPanel];panel.message=@"Open a Virus MIDI bank (.mid) or SysEx preset/bank (.syx).";panel.allowsMultipleSelection=NO;
 if([panel runModal]!=NSModalResponseOK)return;
 NSError* error=nil;NSDictionary* attr=[[NSFileManager defaultManager]attributesOfItemAtPath:panel.URL.path error:&error];
 if([attr[NSFileSize]unsignedLongLongValue]>16*1024*1024){status=@"Bank too large (maximum 16 MiB)";[self setNeedsDisplay:YES];return;}
 NSData* data=[NSData dataWithContentsOfURL:panel.URL options:0 error:&error];
 try{auto bytes=static_cast<const uint8_t*>(data.bytes);auto bank=[panel.URL.pathExtension.lowercaseString isEqual:@"mid"]?virus::patches::parse_midi_bank(bytes,data.length):virus::patches::parse_sysex_bank(bytes,data.length);
  if(bank.patches.empty())throw virus::patches::ParseError("No supported single presets in file");
  NSString* store=[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/VirusTIRework/Patches"];[NSFileManager.defaultManager createDirectoryAtPath:store withIntermediateDirectories:YES attributes:nil error:nil];
  NSString* saved=[store stringByAppendingPathComponent:panel.URL.lastPathComponent];if([NSFileManager.defaultManager fileExistsAtPath:saved])saved=[store stringByAppendingPathComponent:[NSString stringWithFormat:@"%@-%@.%@",panel.URL.lastPathComponent.stringByDeletingPathExtension,NSUUID.UUID.UUIDString,panel.URL.pathExtension]];if(![data writeToFile:saved options:NSDataWritingAtomic error:&error])throw virus::patches::ParseError("Cannot save imported patch file");
  totalPatches+=bank.patches.size();library.push_back({panel.URL.lastPathComponent.stringByDeletingPathExtension.UTF8String,std::move(bank)});chosenBank[item.tag]=library.size()-1;[self rebuild];
 }catch(const std::exception& e){status=[NSString stringWithFormat:@"Bank rejected: %s",e.what()];[self setNeedsDisplay:YES];}
}
- (void)bankChosen:(NSMenuItem*)item {chosenBank[[item.representedObject[@"pane"]integerValue]]=[item.representedObject[@"bank"]integerValue];[self rebuild];}
- (void)showCategories:(NSEvent*)event {
 NSMenu* menu=[[NSMenu alloc]initWithTitle:@"Categories"];
 for(NSInteger n=-1;n<23;++n){NSString* title=n<0?@"All Categories":@(virus::patches::category_name(unsigned(n)));NSMenuItem* item=[[NSMenuItem alloc]initWithTitle:title action:@selector(categoryChosen:) keyEquivalent:@""];item.target=self;item.tag=n;item.state=chosenCategory==n?NSControlStateValueOn:NSControlStateValueOff;[menu addItem:item];}
 [NSMenu popUpContextMenu:menu withEvent:event forView:self];
}
- (void)categoryChosen:(NSMenuItem*)item {chosenCategory=item.tag;search.toolTip=chosenCategory<0?@"Search patch names":[NSString stringWithFormat:@"Category: %s",virus::patches::category_name(unsigned(chosenCategory))];[self rebuild];}
- (void)loadSelection {
 const auto index=selected[activePane];if(index<0||std::size_t(index)>=filtered[activePane].size())return;
 const auto r=filtered[activePane][std::size_t(index)];const auto& patch=library[r.bank].bank.patches[r.patch];NSMutableDictionary* values=[NSMutableDictionary dictionary];for(const auto& v:patch.decoded_values())values[@(v.name)]=@(v.value);
 NSDictionary* payload=@{@"name":@(patch.name.c_str()),@"bank":@(library[r.bank].title.c_str()),@"program":@(patch.program),@"category1":@(virus::patches::category_name(patch.category1)),@"category2":@(virus::patches::category_name(patch.category2)),@"verifiedValues":values,@"checksumVerified":@YES,@"sysex":[NSData dataWithBytes:patch.sysex.data() length:patch.sysex.size()]};
 if(onSelection)onSelection(payload);status=[NSString stringWithFormat:@"%@ recall requested | see connection status",payload[@"name"]];[self setNeedsDisplay:YES];
}
- (void)mouseDown:(NSEvent*)event {
 [self.window makeFirstResponder:self];NSPoint p=[self convertPoint:event.locationInWindow fromView:nil];
 if(NSPointInRect(p,NSMakeRect(529,5,57,22))){[self showCategories:event];return;}
 for(NSInteger pane=0;pane<2;++pane){CGFloat y=pane?219:5;
  if(NSPointInRect(p,NSMakeRect(20,y,456,23))){[self showBanks:pane event:event];return;}
  if(NSPointInRect(p,NSMakeRect(478,y,38,23))&&!library.empty()) {NSInteger delta=p.x<497?-1:1;chosenBank[pane]=chosenBank[pane]<0?(delta<0?NSInteger(library.size())-1:0):(chosenBank[pane]+delta+NSInteger(library.size()))%NSInteger(library.size());[self rebuild];return;}
  NSRect r=paneRect(pane);if(NSPointInRect(p,r)){activePane=pane;NSInteger col=std::min<NSInteger>(7,(p.x-r.origin.x)/83.6),row=std::min<NSInteger>(15,(p.y-r.origin.y)/11);NSUInteger i=offset[pane]+col*16+row;if(i<filtered[pane].size()){selected[pane]=i;if(event.clickCount>=2)[self loadSelection];}[self setNeedsDisplay:YES];return;}
 }
}
- (void)scrollWheel:(NSEvent*)event {
 if(event.scrollingDeltaY==0)return;
 NSPoint p=[self convertPoint:event.locationInWindow fromView:nil];for(NSInteger pane=0;pane<2;++pane)if(NSPointInRect(p,paneRect(pane))){activePane=pane;NSInteger delta=event.scrollingDeltaY>0?-16:16;NSInteger maximum=filtered[pane].size()>128?((filtered[pane].size()-128+15)/16)*16:0;offset[pane]=std::clamp<NSInteger>(NSInteger(offset[pane])+delta,0,maximum);[self setNeedsDisplay:YES];return;}[super scrollWheel:event];
}
- (void)keyDown:(NSEvent*)event {
 if(event.keyCode==36||event.keyCode==76){[self loadSelection];return;}
 NSInteger delta=0;switch(event.keyCode){case 123:delta=-16;break;case 124:delta=16;break;case 125:delta=1;break;case 126:delta=-1;break;default:[super keyDown:event];return;}
 if(filtered[activePane].empty())return;selected[activePane]=std::clamp<NSInteger>(selected[activePane]<0?0:selected[activePane]+delta,0,filtered[activePane].size()-1);
 if(NSUInteger(selected[activePane])<offset[activePane])offset[activePane]=(selected[activePane]/16)*16;
 if(NSUInteger(selected[activePane])>=offset[activePane]+128)offset[activePane]=((selected[activePane]-112)/16)*16;
 [self setNeedsDisplay:YES];
}
@end
