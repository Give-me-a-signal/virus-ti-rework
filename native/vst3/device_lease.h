#pragma once
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdexcept>
#include <string>

namespace VirusRework {
// Close, never explicitly unlock: a recovery child may share this open-file
// description and must retain ownership until its readbacks are durable.
class DeviceLease {
 int fd_=-1;
public:
 DeviceLease()=default;
 DeviceLease(const DeviceLease&)=delete;
 DeviceLease& operator=(const DeviceLease&)=delete;
 ~DeviceLease(){close();}
 void acquire(const std::string& path){
  if(fd_>=0)throw std::runtime_error("Device lease already held");
  int candidate=::open(path.c_str(),O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
  if(candidate<0)throw std::runtime_error("Cannot open device lease");
  if(flock(candidate,LOCK_EX|LOCK_NB)!=0){::close(candidate);throw std::runtime_error("TI owned by another plug-in or its restoration helper; wait for restoration to finish");}
  fd_=candidate;
 }
 void close()noexcept{if(fd_>=0){::close(fd_);fd_=-1;}}
 int descriptor()const noexcept{return fd_;}
 bool held()const noexcept{return fd_>=0;}
};
}
