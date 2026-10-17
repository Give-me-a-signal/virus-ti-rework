#include "restoration.h"
#include "restoration_guard.h"
#include <sys/file.h>
#include <filesystem>
#include <cassert>
#include <iostream>
using namespace VirusRework;
namespace fs=std::filesystem;
void writeFile(const fs::path& p,const PatchBytes& b){std::ofstream f(p,std::ios::binary);f.write((const char*)b.data(),b.size());}
struct FakeDevice{
 RestoreSnapshot data;unsigned writes=0,reads=0;bool settling=false;unsigned settled=0;int fail=-1;bool failed=false;
 void send(const PatchBytes& b){++writes;assert(!settling);if(b.size()==524){assert(b[7]==0);auto p=b[8];if(p==fail){failed=true;throw std::runtime_error("injected write failure");}if(p==64)data.single=b;else{assert(p<16);data.project.parts[p]=b;}}else if(b.size()==267){validateMulti(b);data.project.multi=b;settling=true;}else{assert(b.size()==3||b.size()==11);}}
 PatchBytes single(unsigned p){++reads;return p==64?data.single:data.project.parts[p];}
 PatchBytes multi(){++reads;return data.project.multi;}
};
int main(int argc,char** argv){
 if(argc==3){auto child=fork();if(child<0)return 2;if(child>0)_exit(0);char a='A';assert(write(1,&a,1)==1);char c=0;auto count=read(0,&c,1);if(count==1&&c=='D'){char d='D';write(1,&d,1);return 0;}assert(count==0);assert(fcntl(3,F_GETFD)>=0);assert(flock(3,LOCK_EX|LOCK_NB)==0);usleep(200000);std::ofstream(std::string(argv[1])+"/recovered")<<"yes";return 0;}
 char temporary[]="/tmp/vtr-restoration-XXXXXX";assert(mkdtemp(temporary));fs::path directory=temporary;
 std::ifstream f(STOCK_PROBE,std::ios::binary);PatchBytes stock((std::istreambuf_iterator<char>(f)),{});auto parsed=virus::patches::parse_single(stock);
 RestoreSnapshot original;for(unsigned p=0;p<16;++p){original.project.parts[p]=virus::patches::ram_single(parsed,p);writeFile(directory/("before-part"+std::to_string(p)+".syx"),original.project.parts[p]);}
 original.single=virus::patches::ram_single(parsed,0);original.single[8]=64;unsigned sum=0;for(unsigned i=5;i<265;++i)sum+=original.single[i];original.single[265]=sum&127;sum=0;for(unsigned i=5;i<522;++i)sum+=original.single[i];original.single[522]=sum&127;writeFile(directory/"before-single.syx",original.single);
 original.project.multi.resize(267);auto& m=original.project.multi;m[0]=0xf0;m[2]=0x20;m[3]=0x33;m[4]=1;m[5]=16;m[6]=0x11;m[8]=127;m[266]=0xf7;multiChecksum(m);writeFile(directory/"before-multi.syx",m);
 auto loaded=RestoreSnapshot::read(directory);assert(loaded.project.parts==original.project.parts);
 auto corrupt=original.project.parts[0];corrupt[100]^=1;writeFile(directory/"before-part0.syx",corrupt);bool rejected=false;try{RestoreSnapshot::read(directory);}catch(...){rejected=true;}assert(rejected);writeFile(directory/"before-part0.syx",original.project.parts[0]);
 auto wrong=virus::patches::ram_single(parsed,1);writeFile(directory/"before-part0.syx",wrong);rejected=false;try{RestoreSnapshot::read(directory);}catch(...){rejected=true;}assert(rejected);writeFile(directory/"before-part0.syx",original.project.parts[0]);
 FakeDevice device{original};device.data.single.clear();unsigned saved=0;restoreSnapshot(device,original,[&](auto,auto){++saved;},[&](unsigned ms){if(device.settling){assert(ms>=multiSettleMs);device.settling=false;++device.settled;}});assert(saved==18&&device.data.single==original.single&&device.settled==1);
 device.fail=3;saved=0;rejected=false;try{restoreSnapshot(device,original,[&](auto,auto){++saved;},[&](unsigned ms){if(device.settling){assert(ms>=multiSettleMs);device.settling=false;++device.settled;}});}catch(...){rejected=true;}assert(rejected&&device.failed&&saved==18);
 int lease=open((directory/"lock").c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);assert(lease>=0&&flock(lease,LOCK_EX|LOCK_NB)==0);
 // The shipped helper must reject invalid snapshots before it opens hardware.
 writeFile(directory/"before-part0.syx",corrupt);rejected=false;
 try{RestorationGuard guard;guard.arm(RESTORE_HELPER,directory,1,lease);}catch(...){rejected=true;}
 assert(rejected&&!fs::exists(directory/"restoration-helper"));writeFile(directory/"before-part0.syx",original.project.parts[0]);
 {RestorationGuard guard;guard.arm(fs::absolute(argv[0]),directory,1,lease);guard.complete();assert(!fs::exists(directory/"recovered"));}
 {RestorationGuard guard;guard.arm(fs::absolute(argv[0]),directory,1,lease);close(lease);guard.handoff();}
 int contender=open((directory/"lock").c_str(),O_RDWR);assert(flock(contender,LOCK_EX|LOCK_NB)!=0);
 for(unsigned i=0;i<300&&!fs::exists(directory/"recovered");++i)usleep(10000);assert(fs::exists(directory/"recovered"));
 bool released=false;for(unsigned i=0;i<100;++i){if(flock(contender,LOCK_EX|LOCK_NB)==0){released=true;break;}usleep(10000);}assert(released);close(contender);
 fs::remove_all(directory);std::cout<<"Restoration: snapshot validation, all-part recovery, failure continuation, completion, EOF and exclusive lease passed\n";
}
