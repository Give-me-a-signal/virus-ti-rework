#pragma once
#import <Foundation/Foundation.h>
namespace VirusRework {
BOOL installPlugins(NSString* payload,NSString* library,BOOL vst3,BOOL au,NSError** error);
BOOL uninstallPlugins(NSString* library,BOOL vst3,BOOL au,NSError** error);
}
