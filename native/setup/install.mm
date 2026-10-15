#import "install.h"
#import <CommonCrypto/CommonDigest.h>
#include <stdexcept>
namespace VirusRework {
namespace {
NSError* failed(NSString* text){return [NSError errorWithDomain:@"org.virus-ti-rework.setup" code:1 userInfo:@{NSLocalizedDescriptionKey:text}];}
NSString* sha(NSString* path){NSInputStream* s=[NSInputStream inputStreamWithFileAtPath:path];[s open];CC_SHA256_CTX ctx;CC_SHA256_Init(&ctx);uint8_t bytes[65536];NSInteger count;while((count=[s read:bytes maxLength:sizeof bytes])>0)CC_SHA256_Update(&ctx,bytes,CC_LONG(count));[s close];if(count<0)return nil;uint8_t h[32];CC_SHA256_Final(h,&ctx);NSMutableString* result=[NSMutableString string];for(auto b:h)[result appendFormat:@"%02x",b];return result;}
BOOL verify(NSString* root,NSDictionary* manifest,NSError** error){NSFileManager* fm=NSFileManager.defaultManager;NSUInteger count=0;for(NSString* file in [fm enumeratorAtPath:root]){
 NSString* path=[root stringByAppendingPathComponent:file];NSDictionary* a=[fm attributesOfItemAtPath:path error:error];if([a[NSFileType]isEqual:NSFileTypeSymbolicLink]){if(error)*error=failed(@"Payload contains an unexpected symbolic link.");return NO;}if(![a[NSFileType]isEqual:NSFileTypeRegular])continue;
 NSString* expected=manifest[file];if(![expected isKindOfClass:NSString.class]||![sha(path)isEqual:expected]){if(error)*error=failed(@"Payload checksum verification failed. Download a fresh release.");return NO;}++count;
 }if(!count||count!=manifest.count){if(error)*error=failed(@"Payload manifest is incomplete.");return NO;}
 NSTask* t=[NSTask new];t.executableURL=[NSURL fileURLWithPath:@"/usr/bin/codesign"];t.arguments=@[@"--verify",@"--deep",@"--strict",root];t.standardOutput=[NSFileHandle fileHandleWithNullDevice];t.standardError=[NSFileHandle fileHandleWithNullDevice];if(![t launchAndReturnError:error])return NO;[t waitUntilExit];if(t.terminationStatus){if(error)*error=failed(@"Plug-in signature verification failed.");return NO;}return YES;}
BOOL ours(NSString* path){NSDictionary* info=[NSDictionary dictionaryWithContentsOfFile:[path stringByAppendingPathComponent:@"Contents/Info.plist"]];return [@[@"org.virus-ti-rework.development.vst3",@"org.virus-ti-rework.au"]containsObject:info[@"CFBundleIdentifier"]];}
NSArray* selected(BOOL vst,BOOL au){NSMutableArray* a=[NSMutableArray array];if(vst)[a addObject:@[@"VST3",@"Virus TI Rework.vst3"]];if(au)[a addObject:@[@"Components",@"Virus TI Rework.component"]];return a;}
}
BOOL installPlugins(NSString* payload,NSString* library,BOOL vst,BOOL au,NSError** error){
 NSFileManager* fm=NSFileManager.defaultManager;NSData* data=[NSData dataWithContentsOfFile:[payload stringByAppendingPathComponent:@"checksums.json"]];NSDictionary* manifest=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:error]:nil;
 if(![manifest isKindOfClass:NSDictionary.class]){if(error)*error=failed(@"The release payload manifest is missing.");return NO;}
 NSArray* items=selected(vst,au);if(!items.count){if(error)*error=failed(@"Select at least one format.");return NO;}
 // Verify and stage every chosen format before replacing any installed bundle.
 NSString* stage=[library stringByAppendingPathComponent:[@"Application Support/VirusTIRework/InstallStaging/" stringByAppendingString:NSUUID.UUID.UUIDString]];
 NSMutableArray* installed=[NSMutableArray array];NSMutableArray* backups=[NSMutableArray array];BOOL success=NO;
 @try {
  if(![fm createDirectoryAtPath:stage withIntermediateDirectories:YES attributes:nil error:error])return NO;
  for(NSArray* item in items){NSString* name=item[1];NSDictionary* hashes=manifest[name];if(![hashes isKindOfClass:NSDictionary.class]||!verify([payload stringByAppendingPathComponent:name],hashes,error))return NO;
   if(![fm copyItemAtPath:[payload stringByAppendingPathComponent:name] toPath:[stage stringByAppendingPathComponent:name] error:error]||!verify([stage stringByAppendingPathComponent:name],hashes,error))return NO;}
  NSString* backup=[library stringByAppendingPathComponent:[@"Application Support/VirusTIRework/PluginBackups/" stringByAppendingString:NSUUID.UUID.UUIDString]];
  if(![fm createDirectoryAtPath:backup withIntermediateDirectories:YES attributes:nil error:error])return NO;
  for(NSArray* item in items){NSString* folder=[library stringByAppendingPathComponent:[@"Audio/Plug-Ins/" stringByAppendingString:item[0]]];if(![fm createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:error])return NO;
   NSMutableArray* names=[NSMutableArray arrayWithObject:item[1]];if([item[0]isEqual:@"VST3"])[names addObject:@"Virus TI Rework Development.vst3"];
   for(NSString* name in names){NSString* path=[folder stringByAppendingPathComponent:name];if(![fm fileExistsAtPath:path])continue;if(!ours(path)){if(error)*error=failed(@"An unrelated plug-in occupies the destination; it was not replaced.");return NO;}
    NSString* saved=[backup stringByAppendingPathComponent:name];if(![fm moveItemAtPath:path toPath:saved error:error])return NO;[backups addObject:@[saved,path]];}
   NSString* dest=[folder stringByAppendingPathComponent:item[1]];if(![fm moveItemAtPath:[stage stringByAppendingPathComponent:item[1]] toPath:dest error:error])return NO;[installed addObject:dest];
   if(!verify(dest,manifest[item[1]],error))return NO;
  }success=YES;
 }@finally {
  if(!success){for(NSString* path in installed)[fm removeItemAtPath:path error:nil];for(NSArray* b in [backups reverseObjectEnumerator])[fm moveItemAtPath:b[0] toPath:b[1] error:nil];}
  [fm removeItemAtPath:stage error:nil];
 }return success;
}
BOOL uninstallPlugins(NSString* library,BOOL vst,BOOL au,NSError** error){NSFileManager* fm=NSFileManager.defaultManager;NSString* backup=[library stringByAppendingPathComponent:[@"Application Support/VirusTIRework/PluginBackups/Uninstalled-" stringByAppendingString:NSUUID.UUID.UUIDString]];if(![fm createDirectoryAtPath:backup withIntermediateDirectories:YES attributes:nil error:error])return NO;
 for(NSArray* item in selected(vst,au)){NSMutableArray* names=[NSMutableArray arrayWithObject:item[1]];if([item[0]isEqual:@"VST3"])[names addObject:@"Virus TI Rework Development.vst3"];for(NSString* name in names){NSString* path=[library stringByAppendingPathComponent:[NSString stringWithFormat:@"Audio/Plug-Ins/%@/%@",item[0],name]];if(![fm fileExistsAtPath:path])continue;if(!ours(path)){if(error)*error=failed(@"An unrelated bundle was left in place.");return NO;}if(![fm moveItemAtPath:path toPath:[backup stringByAppendingPathComponent:name] error:error])return NO;}}return YES;
}
}
