#pragma once
#include <cstdint>
#ifdef __OBJC__
@class IOUSBHostObject;
namespace virus::session {
// Synchronous, bounded-time setup helpers for an already-owned interface/device.
// Control executor only, never the audio callback. No open/reset/configuration
// changes here. All outputs remain unchanged on failure; exact transfer lengths
// are required. Return OS error as a diagnostic, not a USB protocol response.
bool read_configuration(IOUSBHostObject*, std::uint8_t&, std::int32_t& error) noexcept;
bool read_alternate(IOUSBHostObject*, std::uint8_t interface_number,
                    std::uint8_t&, std::int32_t& error) noexcept;
bool read_sample_rate(IOUSBHostObject*, std::uint8_t endpoint,
                      std::uint32_t&, std::int32_t& error) noexcept;
bool write_sample_rate(IOUSBHostObject*, std::uint8_t endpoint,
                       std::uint32_t rate, std::int32_t& error) noexcept;
}
#endif
