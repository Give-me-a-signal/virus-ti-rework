#include "virus/iousbhost_backend.hpp"
#import <IOUSBHost/IOUSBHost.h>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"adapter CHECK line %d: %s\n",__LINE__,#x); std::abort(); } } while (0)
using namespace virus;
using namespace virus::audio;
struct TestTransfer {
    NSMutableData* __strong data = nil;
    IOUSBHostIsochronousTransaction* transactions = nullptr;
    IOUSBHostIsochronousTransactionCompletionHandler __strong handler = nil;
    NSUInteger count = 0;
    std::uint64_t frame = 0;
};
// Test-only Objective-C protocol stand-in. Never constructs an IOUSBHost object,
// opens an interface or submits I/O. Production adapter calls its exact selectors.
@interface TestPipe : NSObject {
@public
    IOUSBHostIOSourceDescriptors descriptor;
    std::array<TestTransfer,8> pending;
    bool reject, abort_fail;
    unsigned aborts;
}
@property(readonly) const IOUSBHostIOSourceDescriptors* descriptors;
- (BOOL)enqueueIORequestWithData:(NSMutableData*)data
    transactionList:(IOUSBHostIsochronousTransaction*)list
    transactionListCount:(NSUInteger)count firstFrameNumber:(uint64_t)frame
    options:(IOUSBHostIsochronousTransferOptions)options error:(NSError**)error
    completionHandler:(IOUSBHostIsochronousTransactionCompletionHandler)handler;
- (BOOL)abortWithOption:(IOUSBHostAbortOption)option error:(NSError**)error;
@end
@implementation TestPipe
- (const IOUSBHostIOSourceDescriptors*)descriptors { return &descriptor; }
- (BOOL)enqueueIORequestWithData:(NSMutableData*)data
    transactionList:(IOUSBHostIsochronousTransaction*)list
    transactionListCount:(NSUInteger)count firstFrameNumber:(uint64_t)frame
    options:(IOUSBHostIsochronousTransferOptions)options error:(NSError**)error
    completionHandler:(IOUSBHostIsochronousTransactionCompletionHandler)handler {
    CHECK(options == IOUSBHostIsochronousTransferOptionsNone);
    if (reject) { *error = [NSError errorWithDomain:IOUSBHostErrorDomain code:kIOReturnNoBandwidth userInfo:nil]; return NO; }
    for (auto& p : pending) if (!p.handler) { p = {data,list,handler,count,frame}; return YES; }
    std::abort();
}
- (BOOL)abortWithOption:(IOUSBHostAbortOption)option error:(NSError**)error {
    CHECK(option == IOUSBHostAbortOptionAsynchronous);
    ++aborts;
    if (abort_fail) { *error = [NSError errorWithDomain:IOUSBHostErrorDomain code:kIOReturnError userInfo:nil]; return NO; }
    return YES;
}
@end
Config configuration(usb::Speed speed) {
    const auto ips = speed == usb::Speed::full ? 1000u : 8000u;
    return {speed,{{0,0x81,1,1,256},{48000,ips,2,PcmFormat::signed_16_le,256}},
        {{0,2,1,1,256},{48000,ips,2,PcmFormat::signed_16_le,256}},2,8};
}
TestPipe* pipe_for(const usb::Endpoint& e) {
    auto* pipe = [TestPipe new];
    pipe->descriptor.descriptor.bEndpointAddress = e.address;
    pipe->descriptor.descriptor.bmAttributes = e.attributes;
    pipe->descriptor.descriptor.bInterval = e.interval;
    pipe->descriptor.descriptor.wMaxPacketSize = e.max_packet_size_raw;
    return pipe;
}
void finish(TestPipe* p, std::size_t index, IOReturn result, std::uint32_t bytes) {
    auto t = p->pending[index]; CHECK(t.handler);
    p->pending[index] = {};
    for (std::size_t n = 0; n < t.count; ++n) {
        auto& packet = t.transactions[n];
        packet.status = result; packet.completeCount = bytes; packet.timeStamp = 123456;
        if (bytes <= packet.requestCount) {
            auto* dst = static_cast<std::uint8_t*>(t.data.mutableBytes) + packet.offset;
            for (std::size_t j = 0; j < bytes; ++j) dst[j] = j % 2 ? 0x12 : 0x34;
        }
    }
    t.handler(kIOReturnSuccess,t.transactions);
}
void drain(TestPipe* p) {
    for (std::size_t i = 0; i < p->pending.size(); ++i)
        if (p->pending[i].handler) finish(p,i,kIOReturnAborted,0);
}
void adapter_normal(usb::Speed speed) {
    const auto c = configuration(speed);
    TestPipe* in = pipe_for(c.capture.endpoint);
    TestPipe* out = pipe_for(c.playback.endpoint);
    IOUSBHostBackend b(c,(IOUSBHostPipe*)(id)in,(IOUSBHostPipe*)(id)out);
    AudioFrameQueue capture(2048,2),playback(2048,2);
    Engine e(c,b,capture,playback); b.bind(e);
    CHECK(e.start(speed == usb::Speed::full ? 10 : 80));
    CHECK(in->pending[0].frame == 10 && in->pending[1].frame == (speed == usb::Speed::full ? 18u : 11u));
    CHECK(in->pending[0].count == 8);
    for (std::size_t n = 0; n < 8; ++n) CHECK(in->pending[0].transactions[n].offset == n * 256);
    const auto packet_bytes = speed == usb::Speed::full ? 192u : 24u;
    CHECK(out->pending[0].transactions[0].requestCount == packet_bytes);
    finish(in,1,kIOReturnUnderrun,packet_bytes);
    CHECK(e.stats().capture_frames == 0);
    finish(in,0,kIOReturnUnderrun,packet_bytes);
    CHECK(e.stats().capture_frames == packet_bytes / 4 * 16 && e.stats().capture_timestamp_valid);
    CHECK(e.stats().last_capture_host_ns != 0 && e.stats().packet_errors == 0);
    std::array<std::int32_t,96> samples{};
    CHECK(capture.pop(samples.data(),48,samples.size()) == QueueStatus::ok);
    for (auto sample : samples) CHECK(sample == 0x12340000);
    // Completion pump replenished requests; only one abort per direction.
    e.stop(); CHECK(in->aborts == 1 && out->aborts == 1 && e.outstanding() == 4);
    drain(in); drain(out); CHECK(e.state() == State::idle);
    CHECK(e.start(speed == usb::Speed::full ? 20 : 160));
    e.stop(); drain(in); drain(out); CHECK(e.state() == State::idle);
}
void failures() {
    auto c = configuration(usb::Speed::high);
    TestPipe* in = pipe_for(c.capture.endpoint); TestPipe* out = pipe_for(c.playback.endpoint);
    IOUSBHostBackend b(c,(IOUSBHostPipe*)(id)in,(IOUSBHostPipe*)(id)out);
    AudioFrameQueue capture(2048,2),playback(2048,2);
    Engine e(c,b,capture,playback); b.bind(e);
    CHECK(!e.start(81) && b.last_error() == kIOReturnBadArgument && !e.outstanding());
    in->reject = true;
    CHECK(!e.start(80) && b.last_error() == kIOReturnNoBandwidth && !e.outstanding());
    in->reject = false;
    CHECK(e.start(80));
    in->abort_fail = true; e.stop();
    CHECK(e.outstanding() == 4 && b.last_error() == kIOReturnError);
    drain(in); drain(out); CHECK(e.state() == State::idle);
    in->abort_fail = false;
    CHECK(e.start(160));
    finish(in,0,kIOReturnSuccess,260); // Malformed packet count never copies beyond buffers.
    CHECK(e.failed() && e.stats().malformed_completions == 1);
    drain(in); drain(out); CHECK(e.state() == State::idle);
}
int main() {
    @autoreleasepool {
        adapter_normal(usb::Speed::full); adapter_normal(usb::Speed::high); failures();
    }
    std::puts("IOUSBHost adapter offline selector tests passed: frame mapping, offsets, completion copy/status/timebase, refill, abort/drain, rejection; NO USB objects or hardware used");
}
