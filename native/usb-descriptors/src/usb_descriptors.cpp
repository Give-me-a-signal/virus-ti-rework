#include "virus/usb_descriptors.hpp"
#include <set>
#include <utility>

namespace virus::usb {
ParseError::ParseError(std::size_t where, const char* message)
    : std::runtime_error(message), offset(where) {}
std::optional<unsigned> Endpoint::high_speed_transactions() const {
    if (transfer_type() != 1 && transfer_type() != 3) return std::nullopt;
    const unsigned additional = (max_packet_size_raw >> 11) & 3;
    if (additional == 3 || (max_packet_size_raw & 0xe000)) return std::nullopt;
    return additional + 1;
}
std::optional<unsigned> Endpoint::bytes_per_service_interval(Speed speed) const {
    if (transfer_type() != 1 && transfer_type() != 3) return std::nullopt;
    if (!payload_bytes()) return std::nullopt;
    if (speed == Speed::high) {
        auto transactions = high_speed_transactions();
        if (!transactions || payload_bytes() > 1024) return std::nullopt;
        return payload_bytes() * *transactions;
    }
    if (max_packet_size_raw & 0xf800) return std::nullopt;
    if (speed == Speed::full && payload_bytes() <= (transfer_type() == 1 ? 1023u : 64u))
        return payload_bytes();
    if (speed == Speed::low && transfer_type() == 3 && payload_bytes() <= 8)
        return payload_bytes();
    return std::nullopt;
}
std::optional<std::uint64_t> Endpoint::service_interval_ns(Speed speed) const {
    if (!interval) return std::nullopt;
    const auto type = transfer_type();
    if ((speed == Speed::high && (type == 1 || type == 3)) ||
        (speed == Speed::full && type == 1)) {
        if (interval > 16) return std::nullopt;
        return (speed == Speed::high ? 125000ULL : 1000000ULL) << (interval - 1);
    }
    if ((speed == Speed::full || speed == Speed::low) && type == 3)
        return 1000000ULL * interval;
    return std::nullopt;
}
Configuration parse_configuration(const std::vector<std::uint8_t>& b) {
    auto fail = [](std::size_t at, const char* why) { throw ParseError(at, why); };
    if (b.size() < 9) fail(0, "truncated configuration header");
    if (b[0] != 9 || b[1] != 2) fail(0, "expected 9-byte configuration descriptor");
    const std::size_t total = b[2] | (unsigned(b[3]) << 8);
    if (total != b.size()) fail(2, "wTotalLength differs from supplied blob size");
    if (!b[5]) fail(5, "configuration value zero is reserved");
    Configuration c{b[5], b[7], b[8], b[4], {}, {}};
    std::set<std::pair<unsigned, unsigned>> seen_alternates;
    std::set<unsigned> interfaces;
    std::set<unsigned> endpoints;
    auto finish_alternate = [&] {
        if (!c.alternates.empty() && c.alternates.back().endpoints.size() != c.alternates.back().expected_endpoints)
            fail(c.descriptors[c.alternates.back().descriptor_index].offset, "bNumEndpoints mismatch");
    };
    for (std::size_t offset = 0; offset < total;) {
        if (total - offset < 2) fail(offset, "truncated descriptor header");
        const auto length = b[offset], type = b[offset + 1];
        if (length < 2) fail(offset, "descriptor length below two");
        if (length > total - offset) fail(offset, "descriptor extends beyond configuration");
        const auto index = c.descriptors.size();
        c.descriptors.push_back({offset, {b.begin() + offset, b.begin() + offset + length}});
        if (type == 2 && offset != 0) fail(offset, "nested configuration descriptor");
        if (type == 4) {
            if (length != 9) fail(offset, "interface descriptor length must be nine");
            finish_alternate();
            const auto number = b[offset + 2], alternate = b[offset + 3];
            if (!seen_alternates.emplace(number, alternate).second) fail(offset, "duplicate interface alternate");
            interfaces.insert(number);
            endpoints.clear();
            c.alternates.push_back({index, number, alternate, b[offset + 4],
                b[offset + 5], b[offset + 6], b[offset + 7], {}});
        } else if (type == 5) {
            if (length != 7 && length != 9) fail(offset, "endpoint descriptor length must be seven or nine");
            if (c.alternates.empty()) fail(offset, "endpoint before interface");
            const auto address = b[offset + 2];
            if (!(address & 15) || (address & 0x70)) fail(offset, "invalid endpoint address");
            if (!endpoints.insert(address).second) fail(offset, "duplicate endpoint in alternate");
            c.alternates.back().endpoints.push_back({index, address, b[offset + 3], b[offset + 6],
                static_cast<std::uint16_t>(b[offset + 4] | (unsigned(b[offset + 5]) << 8))});
        }
        offset += length;
    }
    finish_alternate();
    if (interfaces.size() != c.expected_interfaces) fail(4, "bNumInterfaces mismatch");
    for (auto number : interfaces)
        if (!seen_alternates.count({number, 0})) fail(4, "interface missing default alternate zero");
    return c;
}
} // namespace virus::usb
