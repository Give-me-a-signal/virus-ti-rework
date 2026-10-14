#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace virus::midi {
using USBMidiPacket = std::array<std::uint8_t, 4>;
struct USBMidiEvent {
    std::uint8_t cable, cin, size;
    std::array<std::uint8_t, 3> bytes;
};
enum class USBMidiResult { ok, invalid_size, reserved_cin, unavailable_cable,
                           invalid_payload, invalid_padding, output_too_small };

// USB MIDI 1.0 section 4. CIN F intentionally carries ANY individual byte,
// not just realtime. Per-cable StreamParser instances can consume its payload.
inline USBMidiResult decode_usb_midi1_packet(const std::uint8_t* packet,
                                             std::size_t size, USBMidiEvent& out,
                                             std::uint16_t cable_mask = 0xffff) noexcept {
    if (!packet || size != 4) return USBMidiResult::invalid_size;
    const auto cable = static_cast<std::uint8_t>(packet[0] >> 4);
    const auto cin = static_cast<std::uint8_t>(packet[0] & 15);
    constexpr std::uint8_t lengths[]{0,0,2,3,3,1,2,3,3,3,3,3,2,2,3,1};
    const auto count = lengths[cin];
    if (!count) return USBMidiResult::reserved_cin;
    if (!(cable_mask & (std::uint16_t(1) << cable))) return USBMidiResult::unavailable_cable;
    for (unsigned i = count + 1; i < 4; ++i)
        if (packet[i]) return USBMidiResult::invalid_padding;
    const auto a = packet[1], b = packet[2], c = packet[3];
    bool valid = false;
    switch (cin) {
        case 2: valid = (a == 0xf1 || a == 0xf3) && b < 0x80; break;
        case 3: valid = a == 0xf2 && b < 0x80 && c < 0x80; break;
        case 4: valid = (a < 0x80 || a == 0xf0) && b < 0x80 && c < 0x80; break;
        case 5: valid = a == 0xf6 || a == 0xf7; break;
        case 6: valid = (a < 0x80 || a == 0xf0) && b == 0xf7; break;
        case 7: valid = (a < 0x80 || a == 0xf0) && b < 0x80 && c == 0xf7; break;
        case 15: valid = true; break;
        default: valid = (a >> 4) == cin && b < 0x80 && (count == 2 || c < 0x80); break;
    }
    if (!valid) return USBMidiResult::invalid_payload;
    out = {cable, cin, count, {a,b,c}};
    return USBMidiResult::ok;
}

// Whole-transfer validation precedes all output writes. No trailing byte is
// silently discarded; callers decide recovery for malformed USB transfers.
// Input/output must not overlap. Output contains fragments, not joined SysEx.
inline USBMidiResult decode_usb_midi1_transfer(const std::uint8_t* bytes,
                                               std::size_t size,
                                               USBMidiEvent* output,
                                               std::size_t output_capacity,
                                               std::size_t& written,
                                               std::uint16_t cable_mask = 0xffff) noexcept {
    if ((!bytes && size) || size % 4) return USBMidiResult::invalid_size;
    const auto count = size / 4;
    if (count > output_capacity || (!output && count)) return USBMidiResult::output_too_small;
    USBMidiEvent candidate{};
    for (std::size_t i=0; i<count; ++i) {
        const auto result = decode_usb_midi1_packet(bytes + 4*i,4,candidate,cable_mask);
        if (result != USBMidiResult::ok) return result;
    }
    for (std::size_t i=0; i<count; ++i)
        decode_usb_midi1_packet(bytes + 4*i,4,output[i],cable_mask);
    written = count;
    return USBMidiResult::ok;
}

inline USBMidiResult encode_usb_midi1_fragment(unsigned cable, unsigned cin,
                                               const std::uint8_t* bytes,
                                               std::size_t size,
                                               USBMidiPacket& output) noexcept {
    if (cable > 15) return USBMidiResult::unavailable_cable;
    if (cin > 15 || cin < 2) return USBMidiResult::reserved_cin;
    constexpr std::uint8_t lengths[]{0,0,2,3,3,1,2,3,3,3,3,3,2,2,3,1};
    if (!bytes || size != lengths[cin]) return USBMidiResult::invalid_size;
    USBMidiPacket packet{static_cast<std::uint8_t>((cable << 4) | cin),0,0,0};
    for(std::size_t i=0;i<size;++i) packet[1+i]=bytes[i];
    USBMidiEvent event{};
    const auto result=decode_usb_midi1_packet(packet.data(),packet.size(),event);
    if(result==USBMidiResult::ok) output=packet;
    return result;
}

// One complete message with status, or one complete SysEx with F0/F7. Realtime
// interleaving must first be split by StreamParser. No output on failure.
inline USBMidiResult encode_usb_midi1_message(unsigned cable,
                                              const std::uint8_t* bytes,
                                              std::size_t size,
                                              USBMidiPacket* output,
                                              std::size_t output_capacity,
                                              std::size_t& written) noexcept {
    if(cable>15) return USBMidiResult::unavailable_cable;
    if(!bytes || !size) return USBMidiResult::invalid_size;
    const auto status=bytes[0];
    if(status==0xf0) {
        if(size<2 || bytes[size-1]!=0xf7) return USBMidiResult::invalid_payload;
        for(std::size_t i=1;i<size-1;++i)
            if(bytes[i]>=0x80) return USBMidiResult::invalid_payload;
        const auto count=size/3+(size%3!=0);
        if(!output || count>output_capacity) return USBMidiResult::output_too_small;
        for(std::size_t i=0;i<count;++i) {
            const auto offset=i*3;
            const auto remaining=size-offset;
            const auto length=remaining>3 ? 3 : remaining;
            const auto cin=remaining>3 ? 4 : (length+4);
            encode_usb_midi1_fragment(cable,static_cast<unsigned>(cin),bytes+offset,length,output[i]);
        }
        written=count;
        return USBMidiResult::ok;
    }
    unsigned cin=0;
    if(status>=0x80 && status<0xf0) cin=status>>4;
    else if(status==0xf1 || status==0xf3) cin=2;
    else if(status==0xf2) cin=3;
    else if(status==0xf6) cin=5;
    else if(status>=0xf8 && status!=0xf9 && status!=0xfd) cin=15;
    else return USBMidiResult::invalid_payload;
    USBMidiPacket candidate{};
    const auto result=encode_usb_midi1_fragment(cable,cin,bytes,size,candidate);
    if(result!=USBMidiResult::ok) return result;
    if(!output || output_capacity<1) return USBMidiResult::output_too_small;
    output[0]=candidate;
    written=1;
    return USBMidiResult::ok;
}
} // namespace virus::midi
