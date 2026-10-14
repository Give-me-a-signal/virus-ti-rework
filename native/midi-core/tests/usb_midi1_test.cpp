#include "usb_midi1.hpp"
#include "midi_core.hpp"
#include "private_control.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace virus::midi;
void receive(void* context,const Event& event) {
    assert(event.error==Error::none);
    auto& bytes=*static_cast<std::vector<std::uint8_t>*>(context);
    bytes.insert(bytes.end(),event.bytes,event.bytes+event.size);
}
int main() {
    USBMidiEvent event{};
    {
        private_control::WordBytes marker{};
        assert(private_control::encode_timing_marker(0x1234,marker));
        assert((marker==private_control::WordBytes{0x53,0xf5,0x34,0x24}));
        std::uint16_t raw=0;
        for(unsigned tick=0;tick<=0x3fff;++tick) {
            assert(private_control::encode_timing_marker(tick,marker));
            assert(private_control::decode_timing_marker(marker.data(),marker.size(),raw));
            assert(raw==tick);
            // The private extension must remain rejected by the standard codec.
            assert(decode_usb_midi1_packet(marker.data(),marker.size(),event)==USBMidiResult::invalid_payload);
        }
        const auto before=marker;
        assert(!private_control::encode_timing_marker(0x4000,marker) && marker==before);
        for(const auto& bytes:std::vector<private_control::WordBytes>{
                {0x23,0xf5,0,0},{0x53,0xf2,0,0},{0x53,0xf5,128,0},{0x53,0xf5,0,128}})
            assert(!private_control::decode_timing_marker(bytes.data(),bytes.size(),raw));
        assert(!private_control::decode_timing_marker(nullptr,4,raw));
        assert(!private_control::decode_timing_marker(marker.data(),3,raw));
        assert(raw==0x3fff);
    }
    {
        // USB-IF section 4 table 4-2 golden examples.
        const std::uint8_t cc[]{0xba,40,64};
        USBMidiPacket packet{};
        assert(encode_usb_midi1_fragment(10,11,cc,3,packet)==USBMidiResult::ok);
        assert((packet==USBMidiPacket{0xab,0xba,40,64}));
        assert(decode_usb_midi1_packet(packet.data(),4,event)==USBMidiResult::ok);
        assert(event.cable==10 && event.cin==11 && event.size==3);
        const std::uint8_t sysex[]{0xf0,0,1,2,3,0xf7};
        std::array<USBMidiPacket,2> packets{};
        std::size_t written=99;
        assert(encode_usb_midi1_message(3,sysex,sizeof sysex,packets.data(),2,written)==USBMidiResult::ok);
        assert(written==2 && (packets[0]==USBMidiPacket{0x34,0xf0,0,1}));
        assert((packets[1]==USBMidiPacket{0x37,2,3,0xf7}));
        const std::uint8_t shortest[]{0xf0,0xf7};
        assert(encode_usb_midi1_message(0,shortest,2,packets.data(),2,written)==USBMidiResult::ok);
        assert(written==1 && (packets[0]==USBMidiPacket{6,0xf0,0xf7,0}));
    }
    {
        // Every defined channel status, cable, and realtime/system-common form.
        std::array<USBMidiPacket,1> packets{};
        for(unsigned cable=0;cable<16;++cable) {
            for(unsigned status=0x80;status<0xf0;++status) {
                const std::uint8_t bytes[]{static_cast<std::uint8_t>(status),0,127};
                const std::size_t length=(status>>4)==12 || (status>>4)==13 ? 2 : 3;
                std::size_t written=0;
                assert(encode_usb_midi1_message(cable,bytes,length,packets.data(),1,written)==USBMidiResult::ok);
                assert(written==1);
                assert(decode_usb_midi1_packet(packets[0].data(),4,event)==USBMidiResult::ok);
                assert(event.cable==cable && event.size==length && event.bytes[0]==status);
            }
            for(unsigned value=0;value<256;++value) {
                // CIN F permits all bytes, even data/running status/SysEx pieces.
                const auto byte=static_cast<std::uint8_t>(value);
                assert(encode_usb_midi1_fragment(cable,15,&byte,1,packets[0])==USBMidiResult::ok);
                assert(decode_usb_midi1_packet(packets[0].data(),4,event)==USBMidiResult::ok);
                assert(event.bytes[0]==value && event.size==1);
            }
        }
        for(const auto& bytes:std::vector<std::vector<std::uint8_t>>{
            {0xf1,1},{0xf2,1,2},{0xf3,2},{0xf6},{0xf8},{0xfa},{0xfb},{0xfc},{0xfe},{0xff}}) {
            std::size_t written=0;
            assert(encode_usb_midi1_message(0,bytes.data(),bytes.size(),packets.data(),1,written)==USBMidiResult::ok);
            assert(decode_usb_midi1_packet(packets[0].data(),4,event)==USBMidiResult::ok);
            assert(event.size==bytes.size());
        }
    }
    {
        // All terminal CIN lengths, from minimal to bounded long SysEx.
        for(std::size_t length=2;length<=2048;++length) {
            std::vector<std::uint8_t> bytes(length);
            for(std::size_t i=1;i<length-1;++i) bytes[i]=static_cast<std::uint8_t>(i%128);
            bytes[0]=0xf0;bytes.back()=0xf7;
            std::vector<USBMidiPacket> packets((length+2)/3);
            std::size_t written=0;
            assert(encode_usb_midi1_message(15,bytes.data(),bytes.size(),packets.data(),packets.size(),written)==USBMidiResult::ok);
            assert(written==packets.size());
            std::vector<std::uint8_t> recovered;
            StreamParser<2048> parser(receive,&recovered);
            for(const auto& packet:packets) {
                assert(decode_usb_midi1_packet(packet.data(),4,event)==USBMidiResult::ok);
                assert(event.cable==15);
                parser.feed(event.bytes.data(),event.size,0);
            }
            parser.finish();assert(recovered==bytes);
        }
    }
    {
        const std::uint8_t transfer[]{0x09,0x90,60,127, 0x1c,0xc2,3,0, 0xff,0xf8,0,0};
        std::array<USBMidiEvent,3> events{};
        std::size_t written=42;
        assert(decode_usb_midi1_transfer(transfer,sizeof transfer,events.data(),events.size(),written)==USBMidiResult::ok);
        assert(written==3 && events[0].cable==0 && events[1].cable==1 && events[2].cable==15);
        events[0].cable=42;written=42;
        assert(decode_usb_midi1_transfer(transfer,sizeof transfer-1,events.data(),3,written)==USBMidiResult::invalid_size);
        assert(decode_usb_midi1_transfer(transfer,sizeof transfer,events.data(),2,written)==USBMidiResult::output_too_small);
        assert(decode_usb_midi1_transfer(transfer,sizeof transfer,events.data(),3,written,3)==USBMidiResult::unavailable_cable);
        assert(events[0].cable==42 && written==42);
        assert(decode_usb_midi1_transfer(nullptr,0,nullptr,0,written)==USBMidiResult::ok && written==0);
        assert(decode_usb_midi1_packet(nullptr,4,event)==USBMidiResult::invalid_size);
        assert(decode_usb_midi1_packet(transfer,3,event)==USBMidiResult::invalid_size);
        for(unsigned cin=0;cin<2;++cin) {
            const USBMidiPacket packet{static_cast<std::uint8_t>(cin),0,0,0};
            assert(decode_usb_midi1_packet(packet.data(),4,event)==USBMidiResult::reserved_cin);
        }
        for(const auto& packet:std::vector<USBMidiPacket>{{0x0b,0x90,1,2},{0x09,0x90,128,1},
            {0x02,0xf2,1,0},{0x03,0xf1,1,2},{0x04,0xf0,1,0xf7},{0x05,0x01,0,0},
            {0x06,1,2,0},{0x07,1,2,3}})
            assert(decode_usb_midi1_packet(packet.data(),4,event)==USBMidiResult::invalid_payload);
        for(const auto& packet:std::vector<USBMidiPacket>{{0x0c,0xc0,1,2},{0x0f,0xf8,1,0},{0x05,0xf7,0,1}})
            assert(decode_usb_midi1_packet(packet.data(),4,event)==USBMidiResult::invalid_padding);
        USBMidiPacket out{1,2,3,4};const auto before=out;
        const std::uint8_t cc[]{0xb0,40,64};
        written=42;
        assert(encode_usb_midi1_message(16,cc,3,&out,1,written)==USBMidiResult::unavailable_cable);
        assert(encode_usb_midi1_message(0,cc,3,&out,0,written)==USBMidiResult::output_too_small);
        assert(encode_usb_midi1_message(0,cc,2,&out,1,written)==USBMidiResult::invalid_size);
        for(const auto& malformed:std::vector<std::vector<std::uint8_t>>{{0xf0},{0xf0,1},{0xf0,0xf8,0xf7},{0xf7},{0xf4},{0xf9},{0xfd},{40,64}})
            assert(encode_usb_midi1_message(0,malformed.data(),malformed.size(),&out,1,written)!=USBMidiResult::ok);
        assert(out==before && written==42);
    }
    std::cout << "USB MIDI1: golden bytes, all channels/cables, CIN F bytes, 2047 SysEx roundtrips, strict rejection passed\n";
}
