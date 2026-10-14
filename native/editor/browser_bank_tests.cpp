#include "browser_bank.hpp"
#include <cassert>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <iterator>
using namespace virus::patches;
int main(int argc,char**argv){assert(argc==2);std::size_t banks=0,patches=0;std::vector<std::uint8_t> sample;
 for(auto& e:std::filesystem::recursive_directory_iterator(argv[1]))if(e.path().extension()==".mid"){
  std::ifstream f(e.path(),std::ios::binary);std::vector<std::uint8_t>b((std::istreambuf_iterator<char>(f)),{});
  try{auto bank=parse_midi_bank(b.data(),b.size());++banks;patches+=bank.patches.size();if(sample.empty()&&!bank.patches.empty())sample=b;
   for(auto&p:bank.patches){assert(!p.name.empty());auto ram=ram_single(p);assert(ram[7]==0&&ram[8]==0&&ram[5]==16);assert(parse_single(ram).name==p.name);
   if(p.sysex.size()==524){auto corrupt=p.sysex;corrupt[265]^=1;unsigned sum=0;for(size_t j=5;j<522;j++)sum+=corrupt[j];corrupt[522]=sum&127;bool rejected=false;try{parse_single(corrupt);}catch(const ParseError&){rejected=true;}assert(rejected);}
   auto v=p.decoded_values();assert(!v.empty());for(auto&x:v)if(std::string(x.name)=="Filter 1/Cutoff")assert(x.value==p.sysex[49]);}
  }catch(const ParseError&){std::cerr<<"FAILED "<<e.path()<<"\n";throw;}
 }
 assert(banks&&patches);for(std::size_t n=0;n<sample.size();++n){bool reject=false;try{parse_midi_bank(sample.data(),n);}catch(const ParseError&){reject=true;}assert(reject);}
 auto bad=sample;bad[0]='X';bool rejected=false;try{parse_midi_bank(bad.data(),bad.size());}catch(const ParseError&){rejected=true;}assert(rejected);
 // Find a genuine SysEx start and corrupt a seven-bit payload without fixing checksum.
 bad=sample;auto at=std::find(bad.begin(),bad.end(),std::uint8_t(0xf0));assert(at!=bad.end());*(at+30)^=1;rejected=false;try{parse_midi_bank(bad.data(),bad.size());}catch(const ParseError&){rejected=true;}assert(rejected);
 std::cout<<banks<<" shipped banks / "<<patches<<" checksum-verified single patches; truncation/corruption tests passed\n";
}
