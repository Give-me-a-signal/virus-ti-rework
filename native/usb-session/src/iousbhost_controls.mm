#include "virus/iousbhost_controls.hpp"
#import <IOUSBHost/IOUSBHost.h>
#include <cstring>

namespace virus::session {
namespace {
bool endpoint_valid(std::uint8_t endpoint) { return (endpoint & 0x0f) && !(endpoint & 0x70); }
bool request(IOUSBHostObject* object, std::uint8_t type, std::uint8_t code,
             std::uint16_t value, std::uint16_t index, std::uint8_t* bytes,
             std::uint16_t count, std::int32_t& result) noexcept {
    if (!object) { result = kIOReturnBadArgument; return false; }
    IOUSBDeviceRequest setup{};
    setup.bmRequestType = type; setup.bRequest = code;
    setup.wValue = OSSwapHostToLittleInt16(value);
    setup.wIndex = OSSwapHostToLittleInt16(index);
    setup.wLength = OSSwapHostToLittleInt16(count);
    NSMutableData* data = [[NSMutableData alloc] initWithBytes:bytes length:count];
    if (!data) { result = kIOReturnNoMemory; return false; }
    NSError* error = nil;
    NSUInteger transferred = 0;
    const BOOL ok = [object sendDeviceRequest:setup data:data bytesTransferred:&transferred
                            completionTimeout:1.0 error:&error];
    if (!ok) { result = error ? static_cast<std::int32_t>(error.code) : kIOReturnError; return false; }
    if (transferred != count || data.length != count) { result = kIOReturnUnderrun; return false; }
    std::memcpy(bytes,data.bytes,count);
    result = kIOReturnSuccess;
    return true;
}
}
bool read_configuration(IOUSBHostObject* object, std::uint8_t& output, std::int32_t& error) noexcept {
    std::uint8_t value = 0;
    if (!request(object,0x80,8,0,0,&value,1,error)) return false;
    output = value; return true;
}
bool read_alternate(IOUSBHostObject* object, std::uint8_t number, std::uint8_t& output, std::int32_t& error) noexcept {
    std::uint8_t value = 0;
    if (!request(object,0x81,10,0,number,&value,1,error)) return false;
    output = value; return true;
}
bool read_sample_rate(IOUSBHostObject* object, std::uint8_t endpoint, std::uint32_t& output, std::int32_t& error) noexcept {
    if (!endpoint_valid(endpoint)) { error = kIOReturnBadArgument; return false; }
    std::uint8_t value[3]{};
    if (!request(object,0xa2,0x81,0x100,endpoint,value,3,error)) return false;
    const std::uint32_t rate = value[0] | (std::uint32_t(value[1]) << 8) | (std::uint32_t(value[2]) << 16);
    if (!rate) { error = kIOReturnBadArgument; return false; }
    output = rate; return true;
}
bool write_sample_rate(IOUSBHostObject* object, std::uint8_t endpoint, std::uint32_t rate, std::int32_t& error) noexcept {
    // All 24-bit rates are representable for restoring an observed baseline;
    // make_plan separately restricts NEW stream rates to 44.1/48 kHz.
    if (!endpoint_valid(endpoint) || !rate || rate > 0xffffff) { error = kIOReturnBadArgument; return false; }
    std::uint8_t bytes[]{static_cast<std::uint8_t>(rate),static_cast<std::uint8_t>(rate >> 8),static_cast<std::uint8_t>(rate >> 16)};
    return request(object,0x22,1,0x100,endpoint,bytes,3,error);
}
} // namespace virus::session
