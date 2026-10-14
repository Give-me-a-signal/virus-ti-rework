#pragma once
#include <spawn.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <string>
#include <stdexcept>
extern char** environ;
namespace VirusRework {
// Per-connection helper, not a daemon. EOF is a restoration request, including
// when the host exits without calling the VST3 lifecycle. No USB is opened by
// the child until it owns the same hardware.lock as the plug-in.
class RestorationGuard {
 int control=-1, acknowledgment=-1;
 pid_t child=-1;
public:
 RestorationGuard()=default;
 RestorationGuard(const RestorationGuard&)=delete;
 ~RestorationGuard(){handoff();}
 pid_t pid()const{return child;}
 bool armed()const{return control>=0;}
 void arm(const std::string& executable,const std::string& directory,uint64_t registryID,int lease){
  if(armed())throw std::runtime_error("Restoration helper already armed");
  int life[2]{-1,-1},ready[2]{-1,-1};
  auto closePipes=[&]{for(auto p:{life[0],life[1],ready[0],ready[1]})if(p>=0)close(p);};
  if(pipe(life)||pipe(ready)){closePipes();throw std::runtime_error("Cannot create restoration lifeline");}
  for(auto p:{life[0],life[1],ready[0],ready[1]})fcntl(p,F_SETFD,FD_CLOEXEC);
  fcntl(life[1],F_SETNOSIGPIPE,1);
  posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions,life[0],STDIN_FILENO);
  posix_spawn_file_actions_adddup2(&actions,ready[1],STDOUT_FILENO);
  // Inherit the same flock open-file description: no unlock/reacquire gap
  // exists in which a reopened DAW could claim the TI during restoration.
  posix_spawn_file_actions_adddup2(&actions,lease,3);
  posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,(directory+"/restoration-helper.log").c_str(),O_WRONLY|O_CREAT|O_APPEND,0600);
  posix_spawnattr_t attributes;posix_spawnattr_init(&attributes);
  posix_spawnattr_setflags(&attributes,POSIX_SPAWN_CLOEXEC_DEFAULT|POSIX_SPAWN_SETPGROUP);
  posix_spawnattr_setpgroup(&attributes,0);
  auto identity=std::to_string(registryID);
  char* argv[]={const_cast<char*>(executable.c_str()),const_cast<char*>(directory.c_str()),const_cast<char*>(identity.c_str()),nullptr};
  int error=posix_spawn(&child,executable.c_str(),&actions,&attributes,argv,environ);
  posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attributes);
  close(life[0]);life[0]=-1;close(ready[1]);ready[1]=-1;
  if(error){closePipes();child=-1;throw std::runtime_error("Cannot start restoration helper: "+std::to_string(error));}
  pollfd descriptor{ready[0],POLLIN,0};char response=0;
  int polled;do{polled=poll(&descriptor,1,10000);}while(polled<0&&errno==EINTR);
  bool valid=polled>0&&read(ready[0],&response,1)==1&&response=='A';
  if(!valid){kill(-child,SIGKILL);closePipes();while(waitpid(child,nullptr,0)<0&&errno==EINTR){}child=-1;throw std::runtime_error("Restoration helper did not validate the snapshots");}
  // The launcher has forked the independent helper and exited. Reap it now;
  // no thread or callback in this bundle may survive a VST3 unload.
  while(waitpid(child,nullptr,0)<0&&errno==EINTR){}child=-1;
  control=life[1];acknowledgment=ready[0];
 }
 void complete(){
  if(control<0)return;
  char done='D';auto written=write(control,&done,1);close(control);control=-1;
  pollfd descriptor{acknowledgment,POLLIN,0};int result;
  do{result=poll(&descriptor,1,10000);}while(result<0&&errno==EINTR);
  char response=0;bool valid=result>0&&read(acknowledgment,&response,1)==1&&response=='D';
  close(acknowledgment);acknowledgment=-1;
  if(written!=1||!valid)throw std::runtime_error("Restoration helper completion failed");
 }
 void handoff() noexcept{
  if(control>=0){close(control);control=-1;}
  if(acknowledgment>=0){close(acknowledgment);acknowledgment=-1;}
 }
};
}
