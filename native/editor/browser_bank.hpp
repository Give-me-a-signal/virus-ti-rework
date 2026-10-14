#pragma once
#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>
#include <stdexcept>
namespace virus::patches {
struct ParseError : std::runtime_error { using std::runtime_error::runtime_error; };
struct Value { const char* name; int value; };
struct Patch {
 std::string name;
 std::uint8_t bank=0,program=0,category1=0,category2=0;
 std::vector<std::uint8_t> sysex;
 std::vector<Value> decoded_values() const;
};
struct Bank { std::vector<Patch> patches; std::size_t skipped_messages=0; };
Bank parse_midi_bank(const std::uint8_t*,std::size_t);
Bank parse_sysex_bank(const std::uint8_t*,std::size_t);
Patch parse_single(const std::vector<std::uint8_t>&);
std::vector<std::uint8_t> ram_single(const Patch&, unsigned part=0); // selected volatile part edit buffer
const char* category_name(unsigned raw) noexcept;
}
