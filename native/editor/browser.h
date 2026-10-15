#pragma once
#import <Cocoa/Cocoa.h>
NS_ASSUME_NONNULL_BEGIN
// Double-click/Return invokes the handler. Single click only selects a row.
// verifiedValues contains direct XML single-dump mappings whose values satisfy
// XML bounds. sysex carries the complete checked patch; parent handles RAM recall.
typedef void (^VTRPatchSelectionHandler)(NSDictionary* patch);
@interface VTRPatchBrowserView : NSView
- (instancetype)initWithFrame:(NSRect)frame
                    resources:(NSString*)resources
                bankDirectory:(NSString*)bankDirectory
             selectionHandler:(VTRPatchSelectionHandler)handler;
- (void)setHardwareBuffers:(NSArray<NSData*>*)buffers;
- (void)setDarkTheme:(BOOL)dark;
- (void)setOriginalAppearance:(BOOL)original;
@property(nonatomic, readonly) NSUInteger bankCount;
@property(nonatomic, readonly) NSUInteger patchCount;
@property(nonatomic, readonly) NSArray<NSNumber*>* filteredPatchCounts;
@property(nonatomic, readonly) NSString* statusText;
@end
NS_ASSUME_NONNULL_END
