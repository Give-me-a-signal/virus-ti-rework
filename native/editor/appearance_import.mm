#import "appearance_import.h"
#import <CommonCrypto/CommonDigest.h>
#include <stdexcept>
#include <cmath>
@interface VTRXMLTree : NSObject <NSXMLParserDelegate>
@property NSMutableArray* stack;
@property NSMutableDictionary* root;
@property NSUInteger count;
@end
@implementation VTRXMLTree
- (instancetype)init {if((self=[super init]))_stack=[NSMutableArray array];return self;}
- (void)parser:(NSXMLParser*)p didStartElement:(NSString*)name namespaceURI:(NSString*)uri qualifiedName:(NSString*)q attributes:(NSDictionary*)a {
 if(++_count>50000){[p abortParsing];return;}NSMutableDictionary* n=[@{@"type":name,@"attrs":a,@"children":[NSMutableArray array],@"text":[NSMutableString string]} mutableCopy];
 if(_stack.count)[_stack.lastObject[@"children"] addObject:n];else _root=n;[_stack addObject:n];
}
- (void)parser:(NSXMLParser*)p foundCharacters:(NSString*)s {[_stack.lastObject[@"text"] appendString:s];}
- (void)parser:(NSXMLParser*)p didEndElement:(NSString*)n namespaceURI:(NSString*)u qualifiedName:(NSString*)q {[_stack removeLastObject];}
@end
namespace VirusRework {
namespace {
NSError* failure(NSString* text){return [NSError errorWithDomain:@"org.virus-ti-rework.appearance" code:1 userInfo:@{NSLocalizedDescriptionKey:text}];}
NSDictionary* child(NSDictionary* n,NSString* type){for(NSDictionary* c in n[@"children"])if([c[@"type"]isEqual:type])return c;return nil;}
NSDictionary* parse(NSString* path){NSData* data=[NSData dataWithContentsOfFile:path];if(!data||data.length>4*1024*1024)throw std::runtime_error("Missing or oversized appearance XML");
 NSXMLParser* p=[[NSXMLParser alloc]initWithData:data];p.shouldResolveExternalEntities=NO;VTRXMLTree* tree=[VTRXMLTree new];p.delegate=tree;if(![p parse]||!tree.root)throw std::runtime_error("Invalid appearance XML");return tree.root;}
NSDictionary* ast(NSDictionary* c){NSMutableArray* tests=[NSMutableArray array];for(NSDictionary* t in c[@"children"]){NSDictionary* a=t[@"attrs"];[tests addObject:@{@"op":@"test",@"type":a[@"type"]?:@"",@"parameter":a[@"param"]?:@"",@"min":@([a[@"minvalue"]doubleValue]),@"max":@([a[@"maxvalue"]doubleValue])}];}
 NSDictionary* e=@{@"op":c[@"attrs"][@"operand"]?:@"and",@"args":tests};return [c[@"attrs"][@"result"]isEqual:@"invert"]?@{@"op":@"not",@"arg":e}:e;}
void flatten(NSDictionary* tree,NSString* parent,double px,double py,NSMutableArray* nodes,NSString* directory){
 NSDictionary* a=tree[@"attrs"],*s=child(tree,@"Size")[@"attrs"];double x=px+[s[@"left"]doubleValue],y=py+[s[@"top"]doubleValue],w=[(s[@"width"]?:a[@"width"])doubleValue],h=[(s[@"height"]?:a[@"height"])doubleValue];
 if(!std::isfinite(x)||!std::isfinite(y)||w<0||h<0||w>10000||h>10000)throw std::runtime_error("Invalid appearance geometry");
 NSString* ident=[NSString stringWithFormat:@"import%lu",nodes.count];NSMutableDictionary* sprites=[NSMutableDictionary dictionary];
 for(NSString* key in a){NSString* file=a[key];if(![file hasSuffix:@".png"])continue;if(![file.lastPathComponent isEqual:file])throw std::runtime_error("Invalid artwork filename");
  NSBitmapImageRep* im=[NSBitmapImageRep imageRepWithData:[NSData dataWithContentsOfFile:[directory stringByAppendingPathComponent:file]]];if(!im)throw std::runtime_error("Missing or invalid artwork");
  NSMutableDictionary* sprite=[@{@"file":file,@"image_width":@(im.pixelsWide),@"image_height":@(im.pixelsHigh)} mutableCopy];if(w==im.pixelsWide&&h>0&&im.pixelsHigh%int(h)==0)sprite[@"vertical_frame_count"]=@(im.pixelsHigh/int(h));sprites[key]=sprite;
 }
 NSMutableDictionary* metadata=[NSMutableDictionary dictionary];for(NSDictionary* c in tree[@"children"])if([@[@"Size",@"BackColor",@"ForeColor",@"TileInsetRect"]containsObject:c[@"type"]])metadata[c[@"type"]]=c[@"attrs"];
 [nodes addObject:@{@"id":ident,@"parent_id":parent?:NSNull.null,@"type":tree[@"type"],@"attrs":a,@"metadata":metadata,@"sprites":sprites,@"absolute_rect":@{@"left":@(x),@"top":@(y),@"width":@(w),@"height":@(h)}}];
 for(NSDictionary* c in tree[@"children"])if(!metadata[c[@"type"]])flatten(c,ident,x,y,nodes,directory);
}
}
NSString* originalAppearanceDirectory(){return [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/VirusTIRework/Appearance/v1"] ;}
NSDictionary* originalAppearanceModel(NSString* directory,NSError** error){try{
 NSMutableDictionary *params=[NSMutableDictionary dictionary],*conditions=[NSMutableDictionary dictionary],*lists=[NSMutableDictionary dictionary];
 for(NSString* file in @[@"remotedevice.xml",@"editordevice.xml"]){NSDictionary* root=parse([directory stringByAppendingPathComponent:file]);
  for(NSDictionary* group in child(root,@"ParameterSets")[@"children"])for(NSDictionary* p in group[@"children"])params[[NSString stringWithFormat:@"%@/%@",group[@"attrs"][@"name"],p[@"attrs"][@"name"]]]=@{@"attrs":p[@"attrs"]};
  for(NSDictionary* c in child(root,@"Conditionsets")[@"children"])conditions[c[@"attrs"][@"name"]]=@{@"ast":ast(c)};
  for(NSDictionary* l in child(root,@"Valuelists")[@"children"]){NSMutableArray* entries=[NSMutableArray array];for(NSDictionary* e in l[@"children"])[entries addObject:@{@"attrs":e[@"attrs"],@"text":[e[@"text"]stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet]}];lists[l[@"attrs"][@"name"]]=@{@"entries":entries};}
 }
 NSDictionary* editor=parse([directory stringByAppendingPathComponent:@"editor.xml"]);NSMutableDictionary* bitmaps=[NSMutableDictionary dictionary];for(NSDictionary* declaration in child(editor,@"Bitmaps")[@"children"]){NSString* name=declaration[@"attrs"][@"Name"];if(name)bitmaps[name]=@{@"declarations":@[declaration]};}
 NSDictionary* frame=child(editor,@"Frame");if(!frame)throw std::runtime_error("Missing editor frame");NSMutableArray* nodes=[NSMutableArray array];flatten(frame,nil,0,0,nodes,directory);if(nodes.count<100)throw std::runtime_error("Incomplete editor layout");
 return @{@"nodes":nodes,@"parameters":params,@"conditions":conditions,@"valuelists":lists,@"bitmaps":bitmaps};
 }catch(const std::exception& e){if(error)*error=failure(@(e.what()));return nil;}}
BOOL importOriginalAppearance(NSURL* package,NSError** error){NSFileManager* fm=NSFileManager.defaultManager;NSString* work=[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];NSString* staging=nil;BOOL ok=NO;
 @try {
  NSInputStream* input=[NSInputStream inputStreamWithURL:package];[input open];CC_SHA256_CTX ctx;CC_SHA256_Init(&ctx);uint8_t buffer[65536];NSInteger n;while((n=[input read:buffer maxLength:sizeof buffer])>0)CC_SHA256_Update(&ctx,buffer,CC_LONG(n));[input close];
  unsigned char hash[CC_SHA256_DIGEST_LENGTH];CC_SHA256_Final(hash,&ctx);NSMutableString* hex=[NSMutableString string];for(auto b:hash)[hex appendFormat:@"%02x",b];
  if(n<0||![hex isEqual:@"3ebcfef395a6c5f1ef2162404228f1fbe571371f7389836f13c55c8a7636ce1d"]){if(error)*error=failure(@"Select the original Virus TI Software Suite 5.1.7.00.pkg. Its checksum must match the supported Access download.");return NO;}
  [fm createDirectoryAtPath:work withIntermediateDirectories:YES attributes:nil error:error];NSTask* task=[NSTask new];task.executableURL=[NSURL fileURLWithPath:@"/usr/sbin/pkgutil"];task.arguments=@[@"--expand-full",package.path,[work stringByAppendingPathComponent:@"expanded"]];task.standardOutput=[NSFileHandle fileHandleWithNullDevice];task.standardError=[NSFileHandle fileHandleWithNullDevice];
  if(![task launchAndReturnError:error])return NO;[task waitUntilExit];if(task.terminationStatus){if(error)*error=failure(@"The Access package could not be extracted.");return NO;}
  NSString* source=[work stringByAppendingPathComponent:@"expanded/Core_components.pkg/Payload/Library/Application Support/Access Music/Virus TI/Common/Virus Control.bundle/Contents/Resources"];
  NSString* dest=originalAppearanceDirectory();[fm createDirectoryAtPath:dest.stringByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:error];staging=[dest stringByAppendingFormat:@".%@",NSUUID.UUID.UUIDString];[fm createDirectoryAtPath:staging withIntermediateDirectories:YES attributes:nil error:error];
  for(NSString* file in [fm contentsOfDirectoryAtPath:source error:error])if([file hasSuffix:@".png"]||[@[@"editor.xml",@"remotedevice.xml",@"editordevice.xml"]containsObject:file]){if(![fm copyItemAtPath:[source stringByAppendingPathComponent:file] toPath:[staging stringByAppendingPathComponent:file] error:error])return NO;}
  if(!originalAppearanceModel(staging,error))return NO;
  if([fm fileExistsAtPath:dest]){NSURL* result=nil;ok=[fm replaceItemAtURL:[NSURL fileURLWithPath:dest] withItemAtURL:[NSURL fileURLWithPath:staging] backupItemName:nil options:0 resultingItemURL:&result error:error];}
  else ok=[fm moveItemAtPath:staging toPath:dest error:error];
 }@finally{[fm removeItemAtPath:work error:nil];if(staging)[fm removeItemAtPath:staging error:nil];}return ok;
}
BOOL chooseOriginalAppearance(){NSOpenPanel* p=NSOpenPanel.openPanel;p.message=@"Optional: select your original Access installer to import its appearance. The installer will only be extracted, never run.";p.allowedFileTypes=@[@"pkg"];if([p runModal]!=NSModalResponseOK)return NO;NSError* error=nil;BOOL ok=importOriginalAppearance(p.URL,&error);if(!ok){NSAlert* alert=[NSAlert new];alert.messageText=@"Appearance import failed";alert.informativeText=error.localizedDescription?:@"Import failed";[alert runModal];}return ok;}
}
