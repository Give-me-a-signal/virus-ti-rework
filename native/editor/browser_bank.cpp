#include "browser_bank.hpp"
#include <algorithm>
#include <cstring>
namespace virus::patches {
namespace {
#include "browser_mapping.inc"
[[noreturn]] void bad(const char* text){throw ParseError(text);}
struct Reader {
 const std::uint8_t* p;std::size_t size,pos=0;
 std::uint8_t byte(){if(pos>=size)bad("truncated MIDI file");return p[pos++];}
 std::uint32_t be(unsigned n){std::uint32_t v=0;while(n--)v=(v<<8)|byte();return v;}
 std::uint32_t vlq(){std::uint32_t v=0;for(unsigned i=0;i<4;++i){auto b=byte();v=(v<<7)|(b&127);if(!(b&128))return v;}bad("overlong MIDI variable length");}
 void skip(std::size_t n){if(n>size-pos)bad("MIDI chunk outside file");pos+=n;}
};
void receive(std::vector<std::uint8_t>& bytes,Bank& out) {
 if(bytes.size()<7||bytes[0]!=0xf0||bytes.back()!=0xf7)bad("unterminated SysEx");
 if(bytes[1]!=0||bytes[2]!=0x20||bytes[3]!=0x33||bytes[4]!=1||bytes[6]!=0x10){++out.skipped_messages;return;}
 if(bytes.size()!=267&&bytes.size()!=524){++out.skipped_messages;return;}
 for(std::size_t i=1;i+1<bytes.size();++i)if(bytes[i]&128)bad("non-seven-bit Virus patch data");
 unsigned sum=0;for(std::size_t i=5;i+2<bytes.size();++i)sum+=bytes[i];
 if((sum&127)!=bytes[bytes.size()-2])bad("Virus single-dump checksum mismatch");
 if(bytes.size()==524){sum=0;for(std::size_t i=5;i<265;++i)sum+=bytes[i];if((sum&127)!=bytes[265])bad("Virus first-page checksum mismatch");}
 Patch p;p.bank=bytes[7];p.program=bytes[8];p.category1=bytes[260];p.category2=bytes[261];
 for(std::size_t i=249;i<259;++i){const auto c=bytes[i];p.name+=c>=32&&c<=126?static_cast<char>(c):'?';}
 while(!p.name.empty()&&p.name.back()==' ')p.name.pop_back();
 if(p.name.empty())p.name="(unnamed)";
 p.sysex=bytes;out.patches.push_back(std::move(p));
 if(out.patches.size()>16384)bad("bank patch count exceeds limit");
}
void track(Reader& r,Bank& bank) {
 std::uint8_t running=0;std::vector<std::uint8_t> sysex;bool end=false;std::size_t events=0;
 while(r.pos<r.size){
  if(++events>1000000)bad("MIDI event count exceeds limit");
  r.vlq();auto status=r.byte();
  if(status<0x80){if(!running)bad("MIDI running status without prior status");--r.pos;status=running;}
  if(status>=0x80&&status<=0xef){
   if(!sysex.empty())bad("channel event interrupts SysEx continuation");running=status;
   const unsigned count=(status&0xe0)==0xc0?1:2;
   for(unsigned i=0;i<count;++i)if(r.byte()&128)bad("invalid MIDI channel data");
  } else if(status==0xff){
   running=0;const auto type=r.byte();const auto count=r.vlq();r.skip(count);
   if(type==0x2f){if(count||r.pos!=r.size||!sysex.empty())bad("invalid MIDI end-of-track");end=true;break;}
  } else if(status==0xf0||status==0xf7){
   running=0;const auto count=r.vlq();if(count>r.size-r.pos)bad("truncated SysEx event");
   if(status==0xf0){if(!sysex.empty())bad("nested SysEx");sysex.push_back(0xf0);}
   else if(sysex.empty()){r.skip(count);++bank.skipped_messages;continue;} // escaped SMF event, not a patch
   if(count>1048576-sysex.size())bad("SysEx exceeds size limit");
   for(std::uint32_t n=0;n<count;++n){auto b=r.byte();if(b>=0x80&&b!=0xf7)bad("invalid byte in SysEx");if(b==0xf7&&n+1!=count)bad("bytes after SysEx terminator");sysex.push_back(b);}
   if(!sysex.empty()&&sysex.back()==0xf7){receive(sysex,bank);sysex.clear();}
  } else bad("unsupported MIDI status in SMF");
 }
 if(!end)bad("missing MIDI end-of-track");
}
}
Bank parse_midi_bank(const std::uint8_t* data,std::size_t size){
 if(!data||size<14||size>16*1024*1024)bad("invalid bank size");
 Reader r{data,size};if(r.be(4)!=0x4d546864||r.be(4)!=6)bad("invalid MIDI header");
 auto format=r.be(2),tracks=r.be(2),division=r.be(2);
 if(format>1||!tracks||tracks>1024||(format==0&&tracks!=1)||!division)bad("unsupported MIDI file format");
 Bank result;
 for(unsigned i=0;i<tracks;++i){if(r.be(4)!=0x4d54726b)bad("missing MIDI track");auto count=r.be(4);if(count>r.size-r.pos)bad("track length exceeds file");Reader tr{r.p+r.pos,count};track(tr,result);r.skip(count);}
 if(r.pos!=size)bad("trailing bytes after MIDI tracks");return result;
}
Bank parse_sysex_bank(const std::uint8_t* data,std::size_t size){
 if(!data||!size||size>16*1024*1024)bad("invalid SysEx bank size");
 Bank bank;std::size_t pos=0;
 while(pos<size){if(data[pos]!=0xf0)bad("expected complete SysEx messages");auto end=std::find(data+pos,data+size,0xf7);if(end==data+size)bad("unterminated SysEx bank");std::vector<std::uint8_t> bytes(data+pos,end+1);receive(bytes,bank);pos=end-data+1;}
 return bank;
}
Patch parse_single(const std::vector<std::uint8_t>& data){auto bank=parse_sysex_bank(data.data(),data.size());if(bank.patches.size()!=1||bank.skipped_messages)bad("expected one Virus single patch");return bank.patches.front();}
std::vector<std::uint8_t> ram_single(const Patch& patch,unsigned part){
 if(part>=16)bad("part index out of range");
 auto p=parse_single(patch.sysex).sysex;p[5]=0x10;p[7]=0;p[8]=part;
 // Shipped XML: CC page 2/3 are Original Bank/Original Patch metadata.
 // The TI clears these on RAM recall; they do not encode sound parameters.
 p[11]=0;p[12]=0;
 unsigned sum=0;for(std::size_t i=5;i<265;++i)sum+=p[i];p[265]=sum&127;
 if(p.size()==524){sum=0;for(std::size_t i=5;i<522;++i)sum+=p[i];p[522]=sum&127;}return p;
}
std::vector<Value> Patch::decoded_values() const {
 std::vector<Value> result;
 if(sysex.size()!=267&&sysex.size()!=524)return result;
 if(sysex[0]!=0xf0||sysex[1]!=0||sysex[2]!=0x20||sysex[3]!=0x33||sysex[4]!=1||sysex[6]!=0x10||sysex.back()!=0xf7)return result;
 unsigned checksum=0;for(std::size_t i=1;i+1<sysex.size();++i){if(sysex[i]&128)return result;if(i>=5&&i+2<sysex.size())checksum+=sysex[i];}
 if((checksum&127)!=sysex[sysex.size()-2])return result;
 if(sysex.size()==524){unsigned first=0;for(std::size_t i=5;i<265;++i)first+=sysex[i];if((first&127)!=sysex[265])return result;}
 const bool ti=sysex.size()==524;
 for(const auto& m:mappings)if(m.ti==ti&&m.index<sysex.size()-2){
  const int value=sysex[m.index];if(value>=m.minimum&&value<=m.maximum)result.push_back({m.name,value});
 }
 return result;
}
const char* category_name(unsigned raw) noexcept{return raw<23?category_names[raw]:"Unknown";}
}
