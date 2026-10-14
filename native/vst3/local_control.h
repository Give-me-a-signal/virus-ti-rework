#pragma once
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdlib>

namespace VirusRework {
// One bounded line per connection. No network listener and no hardware ownership.
class LocalControl {
 int listener=-1;
 std::string path_;
 std::atomic<bool> stopping{false};
 std::thread thread;
 std::function<std::string(const std::string&)> handler;
 static bool ready(int fd,short events,int ms){pollfd p{fd,events,0};return poll(&p,1,ms)>0&&(p.revents&events);}
 void serve(int fd){
  uid_t uid;gid_t gid;
  if(getpeereid(fd,&uid,&gid)||uid!=getuid())return;
  int one=1;setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one));
  std::string request;
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);
  while(!stopping&&request.size()<4096&&std::chrono::steady_clock::now()<deadline){
   if(!ready(fd,POLLIN,20))continue;
   char buf[256];auto n=recv(fd,buf,sizeof(buf),MSG_DONTWAIT);
   if(n<=0)return;request.append(buf,size_t(n));
   auto end=request.find('\n');if(end==std::string::npos)continue;
   std::string response;
   if(end>=4096||end+1!=request.size())response=R"({"ok":false,"error":"one bounded command per connection"})";
   else try{response=handler(request.substr(0,end));}catch(...){response=R"({"ok":false,"error":"control handler failed"})";}
   response+='\n';size_t sent=0;
   while(sent<response.size()&&!stopping&&ready(fd,POLLOUT,20)){
    auto count=send(fd,response.data()+sent,response.size()-sent,MSG_DONTWAIT);if(count<=0)break;sent+=size_t(count);
   }
   return;
  }
 }
public:
 explicit LocalControl(std::function<std::string(const std::string&)> callback):handler(std::move(callback)){
  std::string dir="/tmp/virus-ti-control-"+std::to_string(getuid());
  if(mkdir(dir.c_str(),0700)&&errno!=EEXIST)throw std::runtime_error("Cannot create control directory");
  struct stat st{};
  if(lstat(dir.c_str(),&st)||!S_ISDIR(st.st_mode)||st.st_uid!=getuid()||(st.st_mode&0777)!=0700)throw std::runtime_error("Control directory must be owned by this user with mode 0700");
  char suffix[64];std::snprintf(suffix,sizeof(suffix),"/%d-%08x.sock",getpid(),arc4random());path_=dir+suffix;
  sockaddr_un address{};address.sun_family=AF_UNIX;
  if(path_.size()>=sizeof(address.sun_path))throw std::runtime_error("Control socket path too long");
  std::memcpy(address.sun_path,path_.c_str(),path_.size()+1);
  listener=socket(AF_UNIX,SOCK_STREAM,0);
  if(listener<0)throw std::runtime_error("Cannot create control socket");
  if(bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))||chmod(path_.c_str(),0600)||listen(listener,8)){
   close(listener);unlink(path_.c_str());throw std::runtime_error("Cannot bind control socket");
  }
  try{thread=std::thread([this]{while(!stopping){if(!ready(listener,POLLIN,50))continue;int fd=accept(listener,nullptr,nullptr);if(fd>=0){serve(fd);close(fd);}}});}
  catch(...){close(listener);unlink(path_.c_str());throw;}
 }
 ~LocalControl(){stopping=true;if(thread.joinable())thread.join();close(listener);unlink(path_.c_str());}
 const std::string& path()const{return path_;}
 LocalControl(const LocalControl&)=delete;
 LocalControl& operator=(const LocalControl&)=delete;
};
}
