#include "local_control.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
using VirusRework::LocalControl;
static int connectTo(const std::string& path){int fd=socket(AF_UNIX,SOCK_STREAM,0);assert(fd>=0);sockaddr_un addr{};addr.sun_family=AF_UNIX;std::strcpy(addr.sun_path,path.c_str());assert(connect(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==0);timeval timeout{2,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));return fd;}
static std::string readResponse(int fd){std::string result;char b[4096];for(;;){auto n=recv(fd,b,sizeof(b),0);if(n<=0)break;result.append(b,n);if(result.back()=='\n')break;}close(fd);return result;}
int main(int argc,char** argv){
 std::atomic<int> handled{0};std::string path;
 {
  LocalControl server([&](const std::string& command){++handled;return command=="status"?R"({"protocol":1,"ok":true,"host_processing":true,"connected":false})":R"({"protocol":1,"ok":false,"error":"unsupported"})";});path=server.path();
  if(argc==2){std::ofstream(argv[1])<<path<<std::endl;std::this_thread::sleep_for(std::chrono::seconds(4));return 0;}
  struct stat st{};assert(!lstat(path.c_str(),&st)&&S_ISSOCK(st.st_mode)&&(st.st_mode&0777)==0600);
  int fd=connectTo(path);send(fd,"sta",3,0);std::this_thread::sleep_for(std::chrono::milliseconds(10));send(fd,"tus\n",4,0);assert(readResponse(fd).find("\"ok\":true")!=std::string::npos);
  fd=connectTo(path);send(fd,"flash\n",6,0);assert(readResponse(fd).find("unsupported")!=std::string::npos);
  fd=connectTo(path);send(fd,"status\nconnect\n",15,0);assert(readResponse(fd).find("one bounded command")!=std::string::npos);assert(handled==2);
  fd=connectTo(path);std::string oversized(4100,'a');send(fd,oversized.data(),oversized.size(),0);assert(readResponse(fd).empty());assert(handled==2);
  fd=connectTo(path);assert(readResponse(fd).empty()); // Idle clients have a bounded lifetime.
  fd=connectTo(path);send(fd,"status\n",7,0);assert(!readResponse(fd).empty());assert(handled==3);
 }
 assert(access(path.c_str(),F_OK)!=0);std::cout<<"Local control: framing, bounds, unknown command, timeout, recovery and cleanup pass\n";
}
