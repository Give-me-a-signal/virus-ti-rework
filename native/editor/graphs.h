#pragma once
#import <Cocoa/Cocoa.h>
#include <vector>

namespace VirusRework::Graphs {
// Caller closes over selected part and its existing parameter/local-state store.
using Read = double (^)(NSString* parameter);
using Image = NSImage* (^)(NSString* filename);
struct Handle {
    NSPoint point{};
    __strong NSString* xParameter = nil;
    __strong NSString* yParameter = nil;
};
struct Geometry {
    std::vector<std::vector<NSPoint>> paths;
    std::vector<Handle> handles;
    bool fillToBaseline = false;
};
bool supports(NSString* type);
Geometry geometry(NSString* type, NSDictionary* attrs, NSRect rect, Read read);
// Draw overlays only. Parent paints original background and supplies clipping.
void draw(NSString* type, NSDictionary* attrs, NSRect rect, Read read, Image image = nullptr);
// Point/rect coordinates are in the same top-left-based editor coordinate space.
NSInteger hit(NSString* type, NSDictionary* attrs, NSRect rect, Read read, NSPoint point);
NSArray<NSString*>* editingParameters(NSString* type, NSDictionary* attrs, NSRect rect, Read read, NSInteger handle);
// Absolute drag mapping returns @{ @"parameter": name, @"value": NSNumber }.
// Parent owns begin/perform/end-edit gestures for all editingParameters entries.
NSArray<NSDictionary*>* edits(NSString* type, NSDictionary* attrs, NSRect rect, Read read, NSInteger handle, NSPoint point);
} // namespace VirusRework::Graphs
