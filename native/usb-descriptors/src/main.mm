#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <IOUSBHost/IOUSBHost.h>
#include "virus/usb_descriptors.hpp"
#include <cstring>

using namespace virus::usb;
static NSString* hex(const std::vector<std::uint8_t>& b) {
    NSMutableString* s = [NSMutableString stringWithCapacity:b.size() * 2];
    for (auto v : b) [s appendFormat:@"%02x", v];
    return s;
}
static NSDictionary* describe(const std::vector<std::uint8_t>& bytes, Speed speed) {
    try {
        const auto c = parse_configuration(bytes);
        NSMutableArray* descriptors = [NSMutableArray array];
        for (const auto& d : c.descriptors)
            [descriptors addObject:@{@"offset": @(d.offset), @"type": @(d.bytes[1]), @"hex": hex(d.bytes)}];
        NSMutableArray* alternates = [NSMutableArray array];
        for (const auto& a : c.alternates) {
            NSMutableArray* endpoints = [NSMutableArray array];
            for (const auto& e : a.endpoints) {
                NSMutableDictionary* item = [@{@"descriptor_index": @(e.descriptor_index),
                    @"address": @(e.address), @"direction": (e.address & 0x80) ? @"in" : @"out",
                    @"attributes_raw": @(e.attributes), @"transfer_type": @(e.transfer_type()),
                    @"wMaxPacketSize": @(e.max_packet_size_raw), @"payload_bytes": @(e.payload_bytes()),
                    @"bInterval": @(e.interval)} mutableCopy];
                if (e.transfer_type() == 1) {
                    item[@"synchronization_type"] = @(e.sync_type());
                    item[@"usage_type"] = @(e.usage_type());
                }
                if (auto n = e.bytes_per_service_interval(speed)) item[@"bytes_per_service_interval"] = @(*n);
                if (auto n = e.service_interval_ns(speed)) item[@"service_interval_ns"] = @(*n);
                [endpoints addObject:item];
            }
            [alternates addObject:@{@"descriptor_index": @(a.descriptor_index),
                @"interface_number": @(a.interface_number), @"alternate_setting": @(a.alternate_setting),
                @"interface_class": @(a.interface_class), @"interface_subclass": @(a.interface_subclass),
                @"interface_protocol": @(a.interface_protocol), @"endpoints": endpoints}];
        }
        return @{@"valid": @YES, @"raw_hex": hex(bytes), @"configuration_value": @(c.value),
            @"attributes_raw": @(c.attributes), @"max_power_raw": @(c.max_power_raw),
            @"descriptors": descriptors, @"alternates": alternates};
    } catch (const ParseError& error) {
        return @{@"valid": @NO, @"error": @(error.what()), @"offset": @(error.offset), @"raw_hex": hex(bytes)};
    }
}
static NSDictionary* apiError(NSError* error) {
    return error ? @{@"domain": error.domain, @"code": @(error.code), @"description": error.localizedDescription}
        : @{@"description": @"IOUSBHost returned no descriptor/object without NSError"};
}
static NSNumber* numberProperty(io_service_t service, CFStringRef key) {
    id value = CFBridgingRelease(IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0));
    return [value isKindOfClass:NSNumber.class] ? value : nil;
}
static NSDictionary* inspect(IOUSBHostDevice* device, bool* failed) {
    NSMutableArray* configurations = [NSMutableArray array];
    NSError* error = nil;
    NSUInteger length = 18;
    const IOUSBDescriptor* descriptor = [device descriptorWithType:static_cast<tIOUSBDescriptorType>(1)
        length:&length index:0 languageID:0 error:&error];
    if (!descriptor || length != 18) {
        *failed = true;
        return @{@"device_descriptor": @{@"valid": @NO, @"error": apiError(error), @"returned_length": @(length)}, @"configurations": configurations};
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(descriptor);
    if (bytes[0] != 18 || bytes[1] != 1 || (bytes[8] | unsigned(bytes[9]) << 8) != 0x133e ||
        ((bytes[10] | unsigned(bytes[11]) << 8) != 0x0815 && (bytes[10] | unsigned(bytes[11]) << 8) != 0x0816) || !bytes[17]) {
        *failed = true;
        return @{@"device_descriptor": @{@"valid": @NO, @"raw_hex": hex({bytes, bytes + length}),
            @"error": @"Unexpected device descriptor identity or configuration count"}, @"configurations": configurations};
    }
    // Fixed descriptor bytes contain string indices only, never string contents.
    // Copy immediately; framework owns the backing memory throughout this session.
    NSDictionary* deviceInfo = @{@"valid": @YES, @"raw_hex": hex({bytes, bytes + length}),
        @"bcdUSB": @(bytes[2] | unsigned(bytes[3]) << 8),
        @"bDeviceClass": @(bytes[4]), @"bDeviceSubClass": @(bytes[5]),
        @"bDeviceProtocol": @(bytes[6]), @"bMaxPacketSize0": @(bytes[7]),
        @"idVendor": @(bytes[8] | unsigned(bytes[9]) << 8),
        @"idProduct": @(bytes[10] | unsigned(bytes[11]) << 8),
        @"bcdDevice": @(bytes[12] | unsigned(bytes[13]) << 8),
        @"bNumConfigurations": @(bytes[17])};
    const auto count = bytes[17];
    for (unsigned index = 0; index < count; ++index) {
        // The API supplies actual returned length, avoiding a blind wTotalLength copy.
        // It may answer from its cache or issue standard GET_DESCRIPTOR requests.
        length = 65535;
        error = nil;
        descriptor = [device descriptorWithType:static_cast<tIOUSBDescriptorType>(2)
            length:&length index:index languageID:0 error:&error];
        if (!descriptor || length < 9 || length > 65535) {
            *failed = true;
            [configurations addObject:@{@"index": @(index), @"error": apiError(error), @"returned_length": @(length)}];
            continue;
        }
        const auto* start = reinterpret_cast<const std::uint8_t*>(descriptor);
        // Speed is deliberately unknown: no interpretation of unverified registry encodings.
        NSMutableDictionary* result = [describe({start, start + length}, Speed::unknown) mutableCopy];
        result[@"index"] = @(index);
        if (![result[@"valid"] boolValue]) *failed = true;
        [configurations addObject:result];
    }
    return @{@"device_descriptor": deviceInfo, @"configurations": configurations};
}
static NSDictionary* discover(bool openDevice, bool* failed) {
    NSMutableArray* results = [NSMutableArray array];
    NSMutableArray* errors = [NSMutableArray array];
    // Match each exact VID/PID in the registry query, before reading properties.
    // A failed property read on a match must be an error, never silent absence.
    for (NSNumber* expectedProduct in @[@0x0815, @0x0816]) {
        io_iterator_t iterator = IO_OBJECT_NULL;
        CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostDevice");
        if (!matching) {
            *failed = true;
            [errors addObject:@{@"operation": @"IOServiceMatching", @"error": @"Could not create matching dictionary"}];
            continue;
        }
        CFDictionarySetValue(matching, CFSTR("idVendor"), (__bridge CFNumberRef)@0x133e);
        CFDictionarySetValue(matching, CFSTR("idProduct"), (__bridge CFNumberRef)expectedProduct);
        auto status = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
        if (status != KERN_SUCCESS) {
            *failed = true;
            [errors addObject:@{@"operation": @"IOServiceGetMatchingServices", @"status": @(status)}];
        } else {
            io_service_t service;
            while ((service = IOIteratorNext(iterator))) {
                auto vendor = numberProperty(service, CFSTR("idVendor"));
                auto product = numberProperty(service, CFSTR("idProduct"));
                if (!vendor || !product || vendor.unsignedIntValue != 0x133e || ![product isEqualToNumber:expectedProduct]) {
                    *failed = true;
                    [errors addObject:@{@"operation": @"matched_registry_identity", @"expected_product_id": expectedProduct,
                        @"error": @"Matched device identity properties unavailable, invalid, or changed"}];
                } else {
                    uint64_t registryID = 0;
                    auto idStatus = IORegistryEntryGetRegistryEntryID(service, &registryID);
                    if (idStatus != KERN_SUCCESS) {
                        *failed = true;
                        [errors addObject:@{@"operation": @"IORegistryEntryGetRegistryEntryID", @"status": @(idStatus), @"product_id": product}];
                    }
                    NSMutableDictionary* item = [@{@"vendor_id": vendor, @"product_id": product,
                        @"registry_id": @(registryID), @"session_opened": @NO} mutableCopy];
                    if (openDevice) {
                        NSError* error = nil;
                        IOUSBHostDevice* device = [[IOUSBHostDevice alloc] initWithIOService:service
                            options:IOUSBHostObjectInitOptionsNone queue:nil error:&error interestHandler:nil];
                        if (!device) { item[@"open_error"] = apiError(error); *failed = true; }
                        else {
                            item[@"session_opened"] = @YES;
                            @try { [item addEntriesFromDictionary:inspect(device, failed)]; }
                            @finally { [device destroy]; }
                        }
                    }
                    [results addObject:item];
                }
                IOObjectRelease(service);
            }
            IOObjectRelease(iterator);
        }
    }
    return @{@"mode": openDevice ? @"inspect_connected" : @"registry_discovery",
        @"devices": results, @"query_errors": errors, @"speed_interpretation": @"unknown",
        @"session_behavior": openDevice ? @"Exclusive device ownership attempted with options None. Descriptor reads can issue GET_DESCRIPTOR. No capture, seize, configure, reset, alternate selection, or streaming." : @"No device session opened."};
}
int main(int argc, const char* argv[]) {
    @autoreleasepool {
        const char* usage = "Usage: virus-usb-descriptors --file CONFIG.bin [--speed unknown|low|full|high|super]\n"
            "       virus-usb-descriptors --discover\n"
            "       virus-usb-descriptors --inspect-connected\n"
            "Default/--help: usage only; never opens hardware. JSON on stdout.\n"
            "--discover reads registry. --inspect-connected claims device ownership and reads descriptors;\n"
            "no capture/seize/configuration/alternate changes or audio I/O. May fail for permissions or ownership.\n"
            "Exit: 0 success, 2 arguments/file/JSON failure, 3 no matching hardware, 4 acquisition/parse failure.\n";
        if (argc == 1 || (argc == 2 && !strcmp(argv[1], "--help"))) { puts(usage); return 0; }
        NSDictionary* report = nil;
        bool failed = false;
        int code = 0;
        if ((argc == 3 || argc == 5) && !strcmp(argv[1], "--file")) {
            Speed speed = Speed::unknown;
            NSString* speedName = @"unknown";
            if (argc == 5) {
                if (strcmp(argv[3], "--speed")) { fputs(usage, stderr); return 2; }
                speedName = @(argv[4]);
                if ([speedName isEqual:@"low"]) speed = Speed::low;
                else if ([speedName isEqual:@"full"]) speed = Speed::full;
                else if ([speedName isEqual:@"high"]) speed = Speed::high;
                else if ([speedName isEqual:@"super"]) speed = Speed::super;
                else if (![speedName isEqual:@"unknown"]) { fputs(usage, stderr); return 2; }
            }
            NSError* error = nil;
            NSFileHandle* file = [NSFileHandle fileHandleForReadingFromURL:[NSURL fileURLWithPath:@(argv[2])] error:&error];
            NSMutableData* data = file ? [NSMutableData data] : nil;
            while (data && data.length < 65536) {
                NSData* chunk = [file readDataUpToLength:65536 - data.length error:&error];
                if (!chunk) { data = nil; break; }
                if (!chunk.length) break;
                [data appendData:chunk];
            }
            [file closeFile];
            if (!data || data.length > 65535) {
                fprintf(stderr, "Cannot read configuration (maximum 65535 bytes): %s\n", error ? error.localizedDescription.UTF8String : "file too large");
                return 2;
            }
            const auto* bytes = static_cast<const std::uint8_t*>(data.bytes);
            std::vector<std::uint8_t> copy;
            if (data.length) copy.assign(bytes, bytes + data.length);
            NSDictionary* config = describe(copy, speed);
            report = @{@"mode": @"offline", @"speed_assumption": speedName, @"configuration": config};
            if (![config[@"valid"] boolValue]) code = 4;
        } else if (argc == 2 && (!strcmp(argv[1], "--discover") || !strcmp(argv[1], "--inspect-connected"))) {
            report = discover(!strcmp(argv[1], "--inspect-connected"), &failed);
            if (failed) code = 4;
            else if (![report[@"devices"] count]) code = 3;
        } else { fputs(usage, stderr); return 2; }
        NSError* error = nil;
        NSData* json = [NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys error:&error];
        if (!json) { fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); return 2; }
        fwrite(json.bytes, 1, json.length, stdout);
        puts("");
        return code;
    }
}
