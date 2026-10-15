#include "device_lease.h"
#include "recall_policy.h"
#include "../private-midi/dump_transaction.h"
#include <filesystem>
#include <cassert>
#include <vector>
#include <iostream>
using namespace VirusRework;
int main(){
 char path[]="/tmp/vtr-lease-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
 DeviceLease owner,loser;owner.acquire(path);bool refused=false;
 try{loser.acquire(path);}catch(...){refused=true;}
 assert(refused&&!loser.held()&&owner.held());
 int inherited=dup(owner.descriptor());owner.close();refused=false;
 try{loser.acquire(path);}catch(...){refused=true;}assert(refused);
 close(inherited);loser.acquire(path);loser.close();unlink(path);
 // Simulate real time: a delayed valid reply, a truncated reply, then a fresh
 // reply after timeout. Requests must be separated by timeout + drain.
 using Clock=std::chrono::steady_clock;Clock::time_point now{};
 unsigned requests=0;std::vector<long long> sent;
 auto read=[&](unsigned successAttempt){return virus::hardware::readDump([&]{++requests;sent.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());},[&]{return requests==successAttempt&&now.time_since_epoch()>=std::chrono::milliseconds(sent.back()+2100)?std::vector<int>{42}:std::vector<int>{};},[&](unsigned ms){now+=std::chrono::milliseconds(ms);},[&]{return now;});};
 assert(read(2)==std::vector<int>{42});assert(requests==2&&sent[1]-sent[0]==4500);
 requests=0;sent.clear();bool timedOut=false;try{read(4);}catch(...){timedOut=true;}assert(timedOut&&requests==3);
 PatchBytes multi(267);multi[0]=0xf0;multi[2]=0x20;multi[3]=0x33;multi[4]=1;multi[5]=16;multi[6]=0x11;multi[8]=127;multi[266]=0xf7;multiChecksum(multi);
 auto actual=multi;actual[10]=1;multiChecksum(actual);verifyMultiRecall(multi,actual);
 actual[185]=13;multiChecksum(actual);bool rejected=false;try{verifyMultiRecall(multi,actual);}catch(...){rejected=true;}assert(rejected);
 assert(multiSettleMs>=250);
 std::cout<<"Lease contention/inheritance, delayed dump retries and multi routing validation passed\n";
}
