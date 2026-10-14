#include "virus/usb_descriptors.hpp"
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
using namespace virus::usb;
static void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
// Invented USB2 audio-like fixture. NOT captured from any Virus device.
static std::vector<std::uint8_t> synthetic() {
    return {9,2,54,0,1,1,0,0x80,50,
        9,4,0,0,0,1,2,0,0,
        9,4,0,1,2,1,2,0,0,
        4,0x24,1,99,
        9,5,0x81,0x05,0,0x14,1,0,0,
        7,5,0x02,0x09,0x80,0,4,
        4,0x25,1,0,
        3,0xfe,77};
}
static void repairLength(std::vector<std::uint8_t>& b) {
    b[2] = static_cast<std::uint8_t>(b.size()); b[3] = static_cast<std::uint8_t>(b.size() >> 8);
}
static void rejects(const std::vector<std::uint8_t>& b, const char* message) {
    try { (void)parse_configuration(b); } catch (const ParseError&) { return; }
    check(false, message);
}
int main() {
    auto b = synthetic();
    check(b.size() == 54, "fixture size");
    auto c = parse_configuration(b);
    check(c.alternates.size() == 2 && c.descriptors.size() == 8, "alternates/raw descriptors");
    check(c.alternates[0].endpoints.empty(), "zero bandwidth alternate");
    check(c.descriptors[3].bytes == std::vector<std::uint8_t>({4,0x24,1,99}), "class-specific bytes preserved");
    const auto e = c.alternates[1].endpoints[0];
    check(e.address == 0x81 && e.sync_type() == 1 && e.usage_type() == 0, "iso attributes");
    check(e.payload_bytes() == 1024 && e.high_speed_transactions() == 3, "high-bandwidth fields");
    check(e.bytes_per_service_interval(Speed::high) == 3072, "HS capacity");
    check(!e.bytes_per_service_interval(Speed::full) && !e.bytes_per_service_interval(Speed::unknown), "speed matters");
    check(e.service_interval_ns(Speed::high) == 125000, "HS interval");
    check(c.alternates[1].endpoints[1].service_interval_ns(Speed::full) == 8000000, "FS iso exponent");
    Endpoint interrupt{0,0x81,3,10,8};
    check(interrupt.service_interval_ns(Speed::low) == 10000000, "LS interrupt linear");
    check(interrupt.bytes_per_service_interval(Speed::low) == 8, "LS capacity");
    interrupt.interval = 0;
    check(!interrupt.service_interval_ns(Speed::high), "zero interval");
    interrupt.interval = 17;
    check(!interrupt.service_interval_ns(Speed::high), "excessive shift");
    interrupt.max_packet_size_raw = 0x1c00;
    check(!interrupt.high_speed_transactions(), "reserved transaction encoding");
    check(!e.bytes_per_service_interval(Speed::super), "USB3 not inferred from USB2 fields");
    for (std::size_t n = 0; n < b.size(); ++n)
        rejects({b.begin(), b.begin() + n}, "accepted truncated fixture");
    auto bad = b; bad.push_back(0); rejects(bad, "accepted trailing data");
    bad = b; bad[27] = 0; rejects(bad, "accepted zero descriptor length");
    bad = b; bad[27] = 1; rejects(bad, "accepted one descriptor length");
    bad = b; bad[27] = 255; rejects(bad, "accepted overrun");
    bad = b; bad[18] = 8; rejects(bad, "accepted short interface");
    bad = b; bad[31] = 6; rejects(bad, "accepted short endpoint");
    bad = b; bad[33] = 0x80; rejects(bad, "accepted endpoint zero");
    bad = b; bad[33] = 0x91; rejects(bad, "accepted endpoint reserved bits");
    bad = b; bad[42] = 0x81; rejects(bad, "accepted duplicate endpoint");
    bad = b; bad[21] = 0; rejects(bad, "accepted duplicate alternate");
    bad = b; bad[22] = 1; rejects(bad, "accepted endpoint count mismatch");
    bad = b; bad[4] = 2; rejects(bad, "accepted interface count mismatch");
    bad = b; bad[12] = 2; rejects(bad, "accepted missing default alternate");
    bad = b; bad[5] = 0; rejects(bad, "accepted zero configuration value");
    bad = {9,2,16,0,0,1,0,0x80,50,7,5,0x81,1,1,0,1}; rejects(bad, "accepted orphan endpoint");
    bad = {9,2,10,0,0,1,0,0x80,50,2}; rejects(bad, "accepted truncated subheader");
    bad = {9,2,18,0,0,1,0,0x80,50,9,2,9,0,0,1,0,0x80,50}; rejects(bad, "accepted nested config");
    // Distinct interfaces, IN/OUT sharing one endpoint number, unknown descriptors.
    auto multi = b;
    multi.insert(multi.end(), {9,4,1,0,0,0xff,0,0,0}); multi[4] = 2; repairLength(multi);
    check(parse_configuration(multi).alternates.size() == 3, "multiple interfaces");
    bad = b; bad[42] = 0x01; check(parse_configuration(bad).alternates[1].endpoints.size() == 2, "IN/OUT same number valid");
    // Deterministic malformed-input exercise under ASan/UBSan, not hardware evidence.
    std::mt19937 rng(0x133e);
    for (unsigned i = 0; i < 20000; ++i) {
        auto mutated = b;
        for (unsigned j = 0, n = 1 + rng() % 5; j < n; ++j) mutated[rng() % mutated.size()] = rng() & 0xff;
        try { (void)parse_configuration(mutated); } catch (const ParseError&) {}
    }
    std::cout << "USB descriptor tests passed (synthetic fixtures; 20000 deterministic mutations).\n";
}
