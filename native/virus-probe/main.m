#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <CoreMIDI/CoreMIDI.h>
#import <CoreAudio/CoreAudio.h>
#import <sys/utsname.h>

// Matching IDs observed in the shipped AccessVirusTI.kext Info.plist.
static const uint32_t Vendor = 4926;
static BOOL relevant(NSString *s) {
    return [s rangeOfString:@"virus" options:NSCaseInsensitiveSearch].location != NSNotFound ||
           [s rangeOfString:@"access music" options:NSCaseInsensitiveSearch].location != NSNotFound;
}
static NSString *ioName(io_registry_entry_t entry) {
    io_name_t name = {0};
    return IORegistryEntryGetName(entry, name) == KERN_SUCCESS ? @(name) : @"";
}
static NSDictionary *registryNode(io_registry_entry_t entry) {
    CFMutableDictionaryRef raw = NULL;
    kern_return_t result = IORegistryEntryCreateCFProperties(entry, &raw, kCFAllocatorDefault, 0);
    NSDictionary *props = raw ? CFBridgingRelease(raw) : @{};
    // Deliberate allowlist: never serialize serial numbers, raw descriptors or arbitrary properties.
    NSArray *keys = @[@"idVendor", @"idProduct", @"bcdDevice", @"bDeviceClass",
        @"bDeviceSubClass", @"bDeviceProtocol", @"bNumConfigurations", @"USB Product Name",
        @"USB Vendor Name", @"locationID", @"USB Address", @"Device Speed",
        @"bConfigurationValue", @"bInterfaceNumber", @"bAlternateSetting", @"bNumEndpoints",
        @"bInterfaceClass", @"bInterfaceSubClass", @"bInterfaceProtocol", @"bEndpointAddress",
        @"bmAttributes", @"wMaxPacketSize", @"bInterval", @"IOProviderClass", @"CFBundleIdentifier"];
    NSMutableDictionary *safe = [NSMutableDictionary dictionary];
    for (NSString *key in keys) {
        id v = props[key];
        if ([v isKindOfClass:NSString.class] || [v isKindOfClass:NSNumber.class]) safe[key] = v;
    }
    io_name_t cls = {0};
    IOObjectGetClass(entry, cls);
    uint64_t ident = 0;
    IORegistryEntryGetRegistryEntryID(entry, &ident);
    return @{@"registry_id": @(ident), @"class": @(cls), @"name": ioName(entry),
             @"properties_status": @(result), @"properties": safe};
}
static NSDictionary *usbSnapshot(void) {
    NSMutableArray *devices = [NSMutableArray array], *errors = [NSMutableArray array];
    NSMutableSet *seen = [NSMutableSet set];
    for (NSString *cls in @[@"IOUSBHostDevice", @"IOUSBDevice"]) {
        io_iterator_t iterator = IO_OBJECT_NULL;
        kern_return_t status = IOServiceGetMatchingServices(kIOMainPortDefault,
            IOServiceMatching(cls.UTF8String), &iterator);
        if (status != KERN_SUCCESS) {
            [errors addObject:@{@"class": cls, @"status": @(status)}];
            continue;
        }
        io_registry_entry_t entry;
        while ((entry = IOIteratorNext(iterator))) {
            NSDictionary *node = registryNode(entry);
            if ([node[@"properties_status"] intValue] != KERN_SUCCESS) {
                [errors addObject:@{@"class": cls, @"operation": @"registry_properties",
                                    @"status": node[@"properties_status"]}];
            }
            NSDictionary *p = node[@"properties"];
            unsigned int product = [p[@"idProduct"] unsignedIntValue];
            if ([p[@"idVendor"] unsignedIntValue] == Vendor && (product == 2069 || product == 2070) &&
                ![seen containsObject:node[@"registry_id"]]) {
                [seen addObject:node[@"registry_id"]];
                NSMutableDictionary *device = [node mutableCopy];
                NSMutableArray *children = [NSMutableArray array];
                io_iterator_t descendants = IO_OBJECT_NULL;
                kern_return_t childStatus = IORegistryEntryCreateIterator(entry, kIOServicePlane,
                    kIORegistryIterateRecursively, &descendants);
                if (childStatus == KERN_SUCCESS) {
                    io_registry_entry_t child;
                    while ((child = IOIteratorNext(descendants))) {
                        [children addObject:registryNode(child)];
                        IOObjectRelease(child);
                    }
                    IOObjectRelease(descendants);
                }
                device[@"descendants_status"] = @(childStatus);
                device[@"registry_descendants"] = children;
                [devices addObject:device];
            }
            IOObjectRelease(entry);
        }
        IOObjectRelease(iterator);
    }
    return @{@"matching_devices": devices, @"query_errors": errors,
             @"observation": devices.count ? @"matching_registry_device_present" :
                (errors.count ? @"incomplete_registry_query" : @"no_matching_registry_device"),
             @"driver_availability": @"not_determined",
             @"descriptor_interrogation_performed": @NO};
}
static NSString *midiString(MIDIObjectRef object, CFStringRef key) {
    CFStringRef value = NULL;
    if (MIDIObjectGetStringProperty(object, key, &value) != noErr || !value) return @"";
    return CFBridgingRelease(value);
}
static NSDictionary *midiSnapshot(void) {
    MIDIClientRef client = 0;
    OSStatus status = MIDIClientCreate(CFSTR("VirusReadOnlyProbe"), NULL, NULL, &client);
    if (status != noErr) return @{@"status": @(status), @"matching_endpoints": @[]};
    NSMutableArray *endpoints = [NSMutableArray array];
    ItemCount counts[2] = {MIDIGetNumberOfSources(), MIDIGetNumberOfDestinations()};
    for (int direction = 0; direction < 2; ++direction) {
        for (ItemCount i = 0; i < counts[direction]; ++i) {
            MIDIEndpointRef endpoint = direction == 0 ? MIDIGetSource(i) : MIDIGetDestination(i);
            NSString *name = midiString(endpoint, kMIDIPropertyName);
            NSString *display = midiString(endpoint, kMIDIPropertyDisplayName);
            NSString *manufacturer = midiString(endpoint, kMIDIPropertyManufacturer);
            if (relevant(name) || relevant(display) || relevant(manufacturer)) {
                SInt32 offline = 0;
                OSStatus offlineStatus = MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyOffline, &offline);
                NSMutableDictionary *record = [@{@"name": name, @"display_name": display,
                    @"manufacturer": manufacturer, @"direction": direction == 0 ? @"source" : @"destination"} mutableCopy];
                if (offlineStatus == noErr) record[@"offline"] = @(offline != 0);
                [endpoints addObject:record];
            }
        }
    }
    MIDIClientDispose(client);
    return @{@"status": @(status), @"matching_endpoints": endpoints,
        @"total_source_count": @(counts[0]), @"total_destination_count": @(counts[1])};
}
static NSString *audioString(AudioObjectID object, AudioObjectPropertySelector selector) {
    AudioObjectPropertyAddress address = {selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    CFStringRef value = NULL;
    UInt32 size = sizeof(value);
    if (AudioObjectGetPropertyData(object, &address, 0, NULL, &size, &value) != noErr || !value) return @"";
    return CFBridgingRelease(value);
}
static NSDictionary *audioSnapshot(void) {
    AudioObjectPropertyAddress address = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &size);
    NSMutableArray *devices = [NSMutableArray array];
    if (status != noErr) return @{@"status": @(status), @"matching_devices": devices};
    NSMutableData *data = [NSMutableData dataWithLength:size];
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, data.mutableBytes);
    if (status != noErr) return @{@"status": @(status), @"matching_devices": devices};
    const AudioDeviceID *ids = data.bytes;
    for (NSUInteger i = 0; i < size / sizeof(AudioDeviceID); ++i) {
        NSString *name = audioString(ids[i], kAudioObjectPropertyName);
        NSString *manufacturer = audioString(ids[i], kAudioObjectPropertyManufacturer);
        if (!relevant(name) && !relevant(manufacturer)) continue;
        NSMutableDictionary *record = [@{@"name": name, @"manufacturer": manufacturer} mutableCopy];
        AudioObjectPropertyAddress rateAddress = {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        Float64 rate = 0;
        UInt32 rateSize = sizeof(rate);
        OSStatus rateStatus = AudioObjectGetPropertyData(ids[i], &rateAddress, 0, NULL, &rateSize, &rate);
        record[@"nominal_rate_status"] = @(rateStatus);
        if (rateStatus == noErr) record[@"nominal_sample_rate_hz"] = @(rate);
        AudioObjectPropertyAddress rangesAddress = {kAudioDevicePropertyAvailableNominalSampleRates, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        UInt32 rangesSize = 0;
        OSStatus rangesStatus = AudioObjectGetPropertyDataSize(ids[i], &rangesAddress, 0, NULL, &rangesSize);
        NSMutableArray *ranges = [NSMutableArray array];
        if (rangesStatus == noErr) {
            NSMutableData *rangeData = [NSMutableData dataWithLength:rangesSize];
            rangesStatus = AudioObjectGetPropertyData(ids[i], &rangesAddress, 0, NULL, &rangesSize, rangeData.mutableBytes);
            if (rangesStatus == noErr) {
                const AudioValueRange *values = rangeData.bytes;
                for (NSUInteger j = 0; j < rangesSize / sizeof(AudioValueRange); ++j)
                    [ranges addObject:@{@"minimum_hz": @(values[j].mMinimum), @"maximum_hz": @(values[j].mMaximum)}];
            }
        }
        record[@"available_rate_ranges_status"] = @(rangesStatus);
        record[@"available_nominal_rate_ranges"] = ranges;
        [devices addObject:record];
    }
    return @{@"status": @(status), @"matching_devices": devices};
}
int main(int argc, const char *argv[]) {
    @autoreleasepool {
        BOOL requireDevice = NO;
        for (int i = 1; i < argc; ++i) {
            if (!strcmp(argv[i], "--help")) {
                puts("Usage: virus-probe [--require-device]\nRead-only IOKit, CoreMIDI and CoreAudio discovery; JSON on stdout.\nExit: 0 successful snapshot (device may be absent), 2 bad arguments/JSON failure,\n3 --require-device with no matching USB device, 4 subsystem query error.\nNo USB interfaces are opened; no audio/MIDI data is sent.");
                return 0;
            } else if (!strcmp(argv[i], "--require-device")) requireDevice = YES;
            else { fprintf(stderr, "Unknown argument: %s\n", argv[i]); return 2; }
        }
        struct utsname host;
        uname(&host);
        NSDictionary *usb = usbSnapshot(), *midi = midiSnapshot(), *audio = audioSnapshot();
        NSDictionary *report = @{@"schema_version": @1,
            @"captured_at_utc": [[NSISO8601DateFormatter new] stringFromDate:NSDate.date],
            @"host": @{@"os_version": NSProcessInfo.processInfo.operatingSystemVersionString,
                        @"machine": @(host.machine)},
            @"matching_ids": @{@"vendor_decimal": @(Vendor), @"vendor_hex": @"0x133e",
                @"products_decimal": @[@2069, @2070], @"products_hex": @[@"0x0815", @"0x0816"],
                @"source": @"Virus TI Software Suite 5.1.7.00.pkg / Driver_package_10.9.pkg / AccessVirusTI.kext / Contents/Info.plist / IOKitPersonalities"},
            @"usb": usb, @"core_midi": midi, @"core_audio": audio,
            @"limitations": @[@"IOKit registry properties are not a USB descriptor interrogation.",
                @"No matching USB device does not establish driver availability or hardware compatibility.",
                @"Audio/MIDI matches use names containing Virus or Access Music; renamed devices may be missed.",
                @"No USB interface is opened; no audio or MIDI I/O, driver installation, firmware or configuration write is performed.",
                @"Serial properties and device UIDs are omitted; names and USB location IDs remain visible."]};
        NSError *error = nil;
        NSData *json = [NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys error:&error];
        if (!json) { fprintf(stderr, "%s\n", error.localizedDescription.UTF8String); return 2; }
        fwrite(json.bytes, 1, json.length, stdout);
        puts("");
        if ([usb[@"query_errors"] count] || [midi[@"status"] intValue] || [audio[@"status"] intValue]) return 4;
        return requireDevice && [usb[@"matching_devices"] count] == 0 ? 3 : 0;
    }
}
