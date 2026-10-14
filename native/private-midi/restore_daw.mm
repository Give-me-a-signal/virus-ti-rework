#include "hardware_midi.hpp"
#include <fstream>
#include <numeric>
#include <iostream>
int main(int argc,char** argv){@autoreleasepool{try{
 if(argc<2||argc>3||(argc==3&&std::string(argv[2])!="--public"))throw std::runtime_error("Provide the saved DAW session snapshot directory [--public]");
 auto load=[&](const char* name,size_t size){std::ifstream f(std::string(argv[1])+"/"+name,std::ios::binary);std::vector<uint8_t>b{std::istreambuf_iterator<char>(f),{}};if(b.size()!=size||b.front()!=0xf0||b.back()!=0xf7||b[7]!=0||b[265]!=(std::accumulate(b.begin()+5,b.begin()+265,0)&127))throw std::runtime_error("Invalid RAM snapshot");return b;};
 auto part=load("before-part0.syx",524),multi=load("before-multi.syx",267),single=load("before-single.syx",524);
 virus::hardware::Midi midi;if(argc==2)midi.usePrivate();
 // Establish a working read path before changing the volatile edit buffers.
 midi.single(64);midi.single(0);midi.multi();
 midi.send(multi);
 midi.send({0xf0,0,0x20,0x33,1,0x10,0x72,0,32,multi[41],0xf7});
 midi.send({0xf0,0,0x20,0x33,1,0x10,0x72,0,33,multi[57],0xf7});
 std::this_thread::sleep_for(std::chrono::milliseconds(300));midi.send(part);std::this_thread::sleep_for(std::chrono::milliseconds(300));
 for(auto check:{std::pair<const char*,std::vector<uint8_t>>{"part0",midi.single(0)},{"multi",midi.multi()},{"single",midi.single(64)}}){auto& expect=std::string(check.first)=="part0"?part:std::string(check.first)=="multi"?multi:single;std::cout<<check.first;for(size_t i=0;i<expect.size();i++)if(expect[i]!=check.second[i])std::cout<<" "<<i<<":"<<unsigned(expect[i])<<"->"<<unsigned(check.second[i]);std::cout<<"\n";auto name=[NSString stringWithFormat:@"%s/repaired-%s.syx",argv[1],check.first];[[NSData dataWithBytes:check.second.data() length:check.second.size()]writeToFile:name atomically:YES];}
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}}
