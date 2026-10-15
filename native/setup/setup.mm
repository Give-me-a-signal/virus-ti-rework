#import <Cocoa/Cocoa.h>
#include "install.h"
#include <string>
#include "../editor/appearance_import.h"
@interface VTRSetup : NSObject <NSApplicationDelegate>
@property NSWindow* window;
@property NSButton* vst;
@property NSButton* au;
@property NSTextField* status;
@end
@implementation VTRSetup
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
 _window=[[NSWindow alloc]initWithContentRect:NSMakeRect(0,0,580,360) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];_window.title=@"Virus TI Rework Setup — 0.2.0 beta 1";[_window center];
 NSTextField* intro=[NSTextField wrappingLabelWithString:@"Install the native Apple Silicon plug-ins for your user account. Quit your DAW before installing or updating. No Access installer is required for Dark or Polar. This beta is not Developer ID signed or notarized."];intro.frame=NSMakeRect(28,224,524,103);[_window.contentView addSubview:intro];
 _vst=[NSButton checkboxWithTitle:@"VST3 — Bitwig, Ableton Live and other VST3 hosts" target:nil action:nil];_vst.frame=NSMakeRect(28,185,520,25);_vst.state=NSControlStateValueOn;[_window.contentView addSubview:_vst];
 _au=[NSButton checkboxWithTitle:@"Audio Unit — AUv2 hosts" target:nil action:nil];_au.frame=NSMakeRect(28,150,520,25);_au.state=NSControlStateValueOn;[_window.contentView addSubview:_au];
 NSArray* labels=@[@"Install / Update",@"Uninstall",@"Import Original Access Appearance…"];SEL actions[]={@selector(install:),@selector(uninstall:),@selector(import:)};
 for(int i=0;i<3;++i){NSButton* b=[NSButton buttonWithTitle:labels[i] target:self action:actions[i]];b.frame=NSMakeRect(i==2?28:28+i*155,i==2?65:108,i==2?310:145,30);[_window.contentView addSubview:b];}
 _status=[NSTextField wrappingLabelWithString:@"Manual installation: copy the included bundles into ~/Library/Audio/Plug-Ins/VST3 and Components, then restart your DAW."];_status.frame=NSMakeRect(28,9,524,48);_status.font=[NSFont systemFontOfSize:11];[_window.contentView addSubview:_status];[_window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
}
- (void)install:(id)sender {NSError* e=nil;BOOL ok=VirusRework::installPlugins([NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"Payload"],[NSHomeDirectory() stringByAppendingPathComponent:@"Library"],_vst.state==NSControlStateValueOn,_au.state==NSControlStateValueOn,&e);_status.stringValue=ok?@"Installed. Restart your DAW and rescan plug-ins. Previous versions are preserved in Application Support/VirusTIRework/PluginBackups.":e.localizedDescription;}
- (void)uninstall:(id)sender {NSError* e=nil;BOOL ok=VirusRework::uninstallPlugins([NSHomeDirectory() stringByAppendingPathComponent:@"Library"],_vst.state==NSControlStateValueOn,_au.state==NSControlStateValueOn,&e);_status.stringValue=ok?@"Selected plug-ins removed. Backups, imported patches, appearances and recovery data were kept.":e.localizedDescription;}
- (void)import:(id)sender {_status.stringValue=VirusRework::chooseOriginalAppearance()?@"Original appearance imported. Choose Original Access in the plug-in editor.":@"Dark and Polar remain available. No original appearance was imported.";}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)app{return YES;}
@end
int main(int argc,char** argv){@autoreleasepool{
 if(argc==2&&std::string(argv[1])=="--install"){NSError* error=nil;BOOL ok=VirusRework::installPlugins([NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"Payload"],[NSHomeDirectory() stringByAppendingPathComponent:@"Library"],YES,YES,&error);if(!ok)fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return ok?0:1;}
NSApplication* app=NSApplication.sharedApplication;[app setActivationPolicy:NSApplicationActivationPolicyRegular];VTRSetup* delegate=[VTRSetup new];app.delegate=delegate;[app run];}return 0;}
