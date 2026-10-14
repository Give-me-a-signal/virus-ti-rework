#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace virus::usb {
enum class Speed { unknown, low, full, high, super };
struct ParseError : std::runtime_error {
    std::size_t offset;
    ParseError(std::size_t where, const char* message);
};
// All descriptor bytes survive parsing, including class-specific and unknown types.
struct Descriptor {
    std::size_t offset;
    std::vector<std::uint8_t> bytes;
};
struct Endpoint {
    std::size_t descriptor_index;
    std::uint8_t address, attributes, interval;
    std::uint16_t max_packet_size_raw;
    unsigned transfer_type() const { return attributes & 3; }
    unsigned sync_type() const { return (attributes >> 2) & 3; }
    unsigned usage_type() const { return (attributes >> 4) & 3; }
    unsigned payload_bytes() const { return max_packet_size_raw & 0x7ff; }
    // Only defined for high-speed periodic endpoints; not USB3 companion bursts.
    std::optional<unsigned> high_speed_transactions() const;
    std::optional<unsigned> bytes_per_service_interval(Speed speed) const;
    std::optional<std::uint64_t> service_interval_ns(Speed speed) const;
};
struct Alternate {
    std::size_t descriptor_index;
    std::uint8_t interface_number, alternate_setting, expected_endpoints;
    std::uint8_t interface_class, interface_subclass, interface_protocol;
    std::vector<Endpoint> endpoints;
};
struct Configuration {
    std::uint8_t value, attributes, max_power_raw, expected_interfaces;
    std::vector<Descriptor> descriptors;
    std::vector<Alternate> alternates;
};
// One complete binary configuration blob, not a concatenated device descriptor dump.
// Throws ParseError for malformed structure/counts; no casts of untrusted buffers.
Configuration parse_configuration(const std::vector<std::uint8_t>& bytes);
} // namespace virus::usb
