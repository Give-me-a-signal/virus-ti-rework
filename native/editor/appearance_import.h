#pragma once
#import <Cocoa/Cocoa.h>
namespace VirusRework {
NSString* originalAppearanceDirectory();
NSDictionary* originalAppearanceModel(NSString* directory, NSError** error);
BOOL importOriginalAppearance(NSURL* package,NSError** error);
BOOL chooseOriginalAppearance();
}
