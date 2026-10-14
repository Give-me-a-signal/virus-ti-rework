#include "private_usb.hpp"
#include <iostream>
int main(int argc,char** argv){@autoreleasepool{try{
 if(argc!=2)throw std::runtime_error("usage: private-midi-probe OUTPUT_DIRECTORY");
 virus::private_midi::PrivateUSB transport;
 NSString* dir=@(argv[1]);[[NSFileManager defaultManager]createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
 [[NSData dataWithBytes:transport.configuration.data() length:transport.configuration.size()]writeToFile:[dir stringByAppendingPathComponent:@"configuration.bin"] atomically:YES];
 NSDictionary* result=@{@"interface_number":@(transport.interfaceNumber),@"registry_id":@(transport.registryID),@"in_endpoint":@(transport.inAddress),@"out_endpoint":@(transport.outAddress),@"in_transfer_type":@(transport.inType),@"out_transfer_type":@(transport.outType),@"pipe_io_sent":@NO,@"configuration_or_alternate_changed":@NO};
 NSData* json=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:nil];[json writeToFile:[dir stringByAppendingPathComponent:@"result.json"] atomically:YES];std::cout.write((const char*)json.bytes,json.length);std::cout<<"\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}}
