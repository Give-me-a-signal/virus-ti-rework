#include "virus/iousbhost_controls.hpp"
#import <IOUSBHost/IOUSBHost.h>
#include <cassert>
#include <cstring>
#include <iostream>
@interface FakeControl : NSObject
@property IOUSBDeviceRequest last;
@property BOOL fail;
@property BOOL shortTransfer;
@property NSUInteger calls;
@property NSData* reply;
@property NSData* sent;
@end
@implementation FakeControl
- (BOOL)sendDeviceRequest:(IOUSBDeviceRequest)r data:(NSMutableData*)data bytesTransferred:(NSUInteger*)count completionTimeout:(NSTimeInterval)timeout error:(NSError**)error {
    assert(timeout==1.0);self.calls++;self.last=r;self.sent=[data copy];
    if(self.fail){*error=[NSError errorWithDomain:@"test" code:kIOReturnNoDevice userInfo:nil];return NO;}
    if(r.bmRequestType&0x80){assert(data.length==self.reply.length);memcpy(data.mutableBytes,self.reply.bytes,data.length);}
    *count=self.shortTransfer ? 0 : data.length;return YES;
}
@end
int main(){@autoreleasepool{
    using namespace virus::session;
    FakeControl* fake=[FakeControl new];auto* object=(IOUSBHostObject*)fake;
    std::int32_t error=0;std::uint8_t byte=99;std::uint32_t rate=123;
    std::uint8_t one=2;fake.reply=[NSData dataWithBytes:&one length:1];
    assert(read_configuration(object,byte,error)&&byte==2&&fake.last.bmRequestType==0x80&&fake.last.bRequest==8&&fake.last.wLength==1);
    assert(read_alternate(object,4,byte,error)&&fake.last.bmRequestType==0x81&&fake.last.bRequest==10&&fake.last.wIndex==4);
    std::uint8_t r[]{0x44,0xac,0};fake.reply=[NSData dataWithBytes:r length:3];
    assert(read_sample_rate(object,0x81,rate,error)&&rate==44100&&fake.last.bmRequestType==0xa2&&fake.last.bRequest==0x81&&fake.last.wValue==0x100&&fake.last.wIndex==0x81);
    assert(write_sample_rate(object,2,48000,error)&&fake.last.bmRequestType==0x22&&fake.last.bRequest==1);
    std::uint8_t wanted[]{0x80,0xbb,0};assert(fake.sent.length==3&&!memcmp(fake.sent.bytes,wanted,3));
    auto calls=fake.calls;assert(!write_sample_rate(object,0,48000,error));assert(!read_sample_rate(object,0x91,rate,error));assert(!write_sample_rate(object,2,0x1000000,error));assert(fake.calls==calls);
    fake.shortTransfer=YES;rate=123;assert(!read_sample_rate(object,0x81,rate,error)&&rate==123&&error==kIOReturnUnderrun);
    fake.shortTransfer=NO;fake.fail=YES;assert(!read_sample_rate(object,0x81,rate,error)&&rate==123&&error==kIOReturnNoDevice);
    assert(!read_configuration(nil,byte,error)&&byte==2);
    std::cout<<"IOUSBHost setup request test doubles passed (no USB object opened)\n";
}}
