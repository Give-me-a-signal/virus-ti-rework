#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace virus::midi::private_control {

// Recovered internal timing marker, NOT a standards-compliant public MIDI
// message. raw_tick is just a 14-bit field: no units/epoch/scheduling inferred.
using WordBytes = std::array<std::uint8_t,4>;
inline bool encode_timing_marker(unsigned raw_tick, WordBytes& output) noexcept {
    if(raw_tick>0x3fff) return false;
    output={0x53,0xf5,static_cast<std::uint8_t>(raw_tick&0x7f),
            static_cast<std::uint8_t>((raw_tick>>7)&0x7f)};
    return true;
}
inline bool decode_timing_marker(const std::uint8_t* bytes,std::size_t size,
                                 std::uint16_t& raw_tick) noexcept {
    if(!bytes || size!=4 || bytes[0]!=0x53 || bytes[1]!=0xf5 ||
       bytes[2]>=0x80 || bytes[3]>=0x80) return false;
    raw_tick=static_cast<std::uint16_t>(bytes[2] | (std::uint16_t(bytes[3])<<7));
    return true;
}
} // namespace virus::midi::private_control
