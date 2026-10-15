#import <Foundation/Foundation.h>
#include "install.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv){@autoreleasepool{assert(argc==2);NSFileManager* fm=NSFileManager.defaultManager;NSString* root=[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];NSString* lib=[root stringByAppendingPathComponent:@"Library"];NSError* error=nil;
 @try {
 assert(VirusRework::installPlugins(@(argv[1]),lib,YES,YES,&error));NSString* vst=[lib stringByAppendingPathComponent:@"Audio/Plug-Ins/VST3/Virus TI Rework.vst3"],*au=[lib stringByAppendingPathComponent:@"Audio/Plug-Ins/Components/Virus TI Rework.component"];
 NSString* marker=[vst stringByAppendingPathComponent:@"old-install-marker"];assert([@"old" writeToFile:marker atomically:YES encoding:NSUTF8StringEncoding error:&error]);
 // A foreign AU makes the second half fail, after replacing VST3: VST3 must roll back.
 NSString* plist=[au stringByAppendingPathComponent:@"Contents/Info.plist"];NSMutableDictionary* info=[[NSDictionary dictionaryWithContentsOfFile:plist]mutableCopy];info[@"CFBundleIdentifier"]=@"example.other";assert([info writeToFile:plist atomically:YES]);
 assert(!VirusRework::installPlugins(@(argv[1]),lib,YES,YES,&error));assert([fm fileExistsAtPath:marker]);
 info[@"CFBundleIdentifier"]=@"org.virus-ti-rework.au";assert([info writeToFile:plist atomically:YES]);assert(VirusRework::installPlugins(@(argv[1]),lib,YES,YES,&error));assert(![fm fileExistsAtPath:marker]);
 NSString* old=[lib stringByAppendingPathComponent:@"Audio/Plug-Ins/VST3/Virus TI Rework Development.vst3"];assert([fm moveItemAtPath:vst toPath:old error:&error]);assert(VirusRework::installPlugins(@(argv[1]),lib,YES,NO,&error));assert(![fm fileExistsAtPath:old]);
 NSString* data=[lib stringByAppendingPathComponent:@"Application Support/VirusTIRework/Patches/keep.txt"];[fm createDirectoryAtPath:data.stringByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:nil];assert([@"user data" writeToFile:data atomically:YES encoding:NSUTF8StringEncoding error:&error]);
 assert(VirusRework::uninstallPlugins(lib,YES,YES,&error));assert(![fm fileExistsAtPath:vst]&&![fm fileExistsAtPath:au]&&[fm fileExistsAtPath:data]);
 NSString* damaged=[root stringByAppendingPathComponent:@"damaged"];assert([fm copyItemAtPath:@(argv[1]) toPath:damaged error:&error]);NSString* binary=[damaged stringByAppendingPathComponent:@"Virus TI Rework.vst3/Contents/MacOS/Virus TI Rework"];assert([@"bad" writeToFile:binary atomically:YES encoding:NSUTF8StringEncoding error:&error]);assert(!VirusRework::installPlugins(damaged,lib,YES,NO,&error));assert(![fm fileExistsAtPath:vst]);
 std::cout<<"Setup: install, update, rollback, Development migration, uninstall preservation and corrupt payload rejection passed\n";
 }@finally{[fm removeItemAtPath:root error:nil];}
}return 0;}
