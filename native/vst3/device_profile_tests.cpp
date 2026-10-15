#include "restoration.h"
#include "project_recall.h"
#include "midi_safety.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <map>
#include <cstdlib>
#include <unistd.h>
#ifdef RESTORE_HELPER
#include "restoration_guard.h"
#include <sys/file.h>
#endif
using namespace VirusRework;
namespace fs=std::filesystem;
template<class F> void rejects(F f,const char* text){bool caught=false;try{f();}catch(const std::exception& e){caught=true;assert(std::string(e.what()).find(text)!=std::string::npos);}assert(caught);}
PatchBytes sound(unsigned part,unsigned value=21){
 PatchBytes b(524);b[0]=0xf0;b[2]=0x20;b[3]=0x33;b[4]=1;b[5]=16;b[6]=16;b[8]=part;b[9]=12;b[49]=value;b.back()=0xf7;
 const std::string name="SYNTHETIC ";std::copy(name.begin(),name.end(),b.begin()+249);
 unsigned sum=0;for(size_t i=5;i<265;++i)sum+=b[i];b[265]=sum&127;sum=0;for(size_t i=5;i<522;++i)sum+=b[i];b[522]=sum&127;return b;
}
RestoreSnapshot snapshot(DeviceModel model){
 RestoreSnapshot s;s.model=model;s.single=sound(64);
 for(unsigned p=0;p<partCount(model);++p)s.project.parts[p]=sound(p,30+p);
 auto& m=s.project.multi;m.resize(267);m[0]=0xf0;m[2]=0x20;m[3]=0x33;m[4]=1;m[5]=16;m[6]=17;m[8]=127;m.back()=0xf7;
 // Distinct reserved/inactive bytes catch accidental four-wide repacking.
 for(size_t i=9;i<265;++i)m[i]=uint8_t((i*11)%128);multiChecksum(m);s.validate();return s;
}
struct Device {
 RestoreSnapshot state;std::vector<unsigned> requested,written;std::vector<PatchBytes> wire;
 int failPart=-1;bool badMulti=false;int badSound=-1;bool pendingMulti=false;
 explicit Device(RestoreSnapshot initial):state(std::move(initial)){}
 void send(const PatchBytes& b){
  validateMidi(b);wire.push_back(b);
  if(pendingMulti)assert(false&&"command sent before multi settling");
  if(b.size()==524){const auto p=b[8];assert(p==64||p<partCount(state.model));written.push_back(p);if(int(p)==failPart)throw std::runtime_error("injected part write failure");if(p==64)state.single=b;else state.project.parts[p]=b;}
  else if(b.size()==267){validateMulti(b);state.project.multi=b;pendingMulti=true;}
  else if(b.size()==3){assert((b[0]&15)<partCount(state.model));}
  else if(b[6]==0x72){assert(state.model==DeviceModel::ti16&&b[7]<16);}
 }
 void pause(unsigned ms){if(pendingMulti){assert(ms>=250);pendingMulti=false;}}
 PatchBytes single(unsigned p){assert(!pendingMulti);assert(p==64||p<partCount(state.model));requested.push_back(p);auto b=p==64?state.single:state.project.parts[p];if(int(p)==badSound){b[10]^=1;unsigned a=0,c=0;for(size_t i=5;i<265;++i)a+=b[i];b[265]=a&127;for(size_t i=5;i<522;++i)c+=b[i];b[522]=c&127;}return b;}
 PatchBytes multi(){assert(!pendingMulti);auto m=state.project.multi;if(badMulti){m[185]^=1;multiChecksum(m);}return m;}
};
void file(const fs::path& path,const PatchBytes& bytes){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());assert(out.good());}
int main(){
 assert(partCount(DeviceModel::snow4)==4&&partCount(DeviceModel::ti16)==16);
 requirePart(DeviceModel::snow4,3);rejects([]{requirePart(DeviceModel::snow4,4);},"outside");
 rejects([]{partCount(static_cast<DeviceModel>(99));},"Unknown");
 requireVerifiedProfile(DeviceModel::ti16);rejects([]{requireVerifiedProfile(DeviceModel::snow4);},"unverified");
 rejects([]{parseProfile("VTR-PROFILE-1 snow5\n");},"Invalid");
 rejects([]{parseProfile("VTR-PROFILE-1 snow4\nextra");},"Invalid");
 char temporary[]="/tmp/vtr-four-part-XXXXXX";assert(mkdtemp(temporary));const fs::path directory=temporary;
 struct Cleanup {fs::path p;~Cleanup(){fs::remove_all(p);}} cleanup{directory};
 for(auto model:{DeviceModel::snow4,DeviceModel::ti16}){
  const auto root=directory/(model==DeviceModel::snow4?"snow":"ti");fs::create_directory(root);
  auto original=snapshot(model);Device device{original};std::map<std::string,PatchBytes> files;
  auto captured=captureSnapshot(device,model,[&](auto name,auto bytes){files.emplace(name,bytes);file(root/name,bytes);},[]{});
  assert(captured.model==model&&captured.project.parts==original.project.parts);
  assert(device.wire.empty()&&device.requested.size()==partCount(model)+1);
  assert(files.size()==partCount(model)+3); // sound buffers plus model record
  auto restored=RestoreSnapshot::read(root);assert(restored.project.parts==original.project.parts&&restored.model==model);
  assert(restored.project.multi==original.project.multi&&restored.single==original.single);
  device.state.single=sound(64,99);unsigned saved=0;
  restoreSnapshot(device,restored,[&](auto,auto){++saved;},[&](auto ms){device.pause(ms);});
  assert(saved==partCount(model)+2&&device.state.single==original.single);
  assert(device.state.project.multi==original.project.multi&&device.state.project.parts==original.project.parts);
  for(unsigned p:device.written)assert(p==64||p<partCount(model));
  // Failed part writes are reported and do not abandon remaining readbacks.
  device.failPart=1;saved=0;rejects([&]{restoreSnapshot(device,restored,[&](auto,auto){++saved;},[&](auto ms){device.pause(ms);});},"injected part write");
  assert(saved==partCount(model)+2);device.failPart=-1;
  // Bad originals fail before notes, mode changes or any write.
  auto corrupt=restored;corrupt.project.parts[0][49]^=1;auto before=device.wire.size();
  rejects([&]{restoreSnapshot(device,corrupt,[](auto,auto){},[](auto){});},"checksum");assert(device.wire.size()==before);
  // Repeated four-/sixteen-part recalls never fill inactive serialized slots.
  for(unsigned repeat=0;repeat<10;++repeat){
   auto desired=original.project;for(unsigned p=0;p<partCount(model);++p)desired.parts[p]=sound(p,60+repeat);
   auto applied=recallProject(device,desired,model,[&](auto ms){device.pause(ms);},[&](auto bytes,unsigned p){device.send(bytes);return device.single(p);});
   assert(applied.parts==desired.parts&&applied.multi==desired.multi);
   for(unsigned p=partCount(model);p<16;++p)assert(applied.parts[p].empty());
  }
  device.badMulti=true;before=device.written.size();
  rejects([&]{recallProject(device,original.project,model,[&](auto ms){device.pause(ms);},[&](auto b,unsigned p){device.send(b);return device.single(p);});},"routing mismatch");
  assert(device.written.size()==before);device.badMulti=false;
  if(model==DeviceModel::snow4){
#ifdef RESTORE_HELPER
   // Even a valid six-buffer manifest must not enable physical Snow recovery.
   int lease=open((root/"lock").c_str(),O_CREAT|O_RDWR|O_CLOEXEC,0600);assert(lease>=0&&flock(lease,LOCK_EX|LOCK_NB)==0);
   rejects([&]{RestorationGuard guard;guard.arm(RESTORE_HELPER,root,1,lease);},"did not validate");
   std::ifstream status(root/"restoration-status.json");const std::string report(std::istreambuf_iterator<char>(status),{});assert(report.find("unverified")!=std::string::npos);
   assert(!fs::exists(root/"restoration-helper"));close(lease);
#endif
   auto cross=original.project;cross.parts[4]=sound(4);before=device.wire.size();
   rejects([&]{recallProject(device,cross,model,[](auto){},[](auto,unsigned){return PatchBytes{};});},"outside");assert(before==device.wire.size());
   auto extra=original;extra.project.parts[15]=sound(15);rejects([&]{extra.validate();},"outside");
   device.badSound=2;rejects([&]{restoreSnapshot(device,original,[](auto,auto){},[&](auto ms){device.pause(ms);});},"Snow part sound mismatch");device.badSound=-1;
   file(root/"before-part4.syx",sound(4));rejects([&]{RestoreSnapshot::read(root);},"inactive");fs::remove(root/"before-part4.syx");
   fs::remove(root/"before-part3.syx");rejects([&]{RestoreSnapshot::read(root);},"Invalid restoration file");file(root/"before-part3.syx",original.project.parts[3]);
   const auto marker=files.at("before-device-profile.txt");fs::remove(root/"before-device-profile.txt");rejects([&]{RestoreSnapshot::read(root);},"Invalid restoration file");file(root/"before-device-profile.txt",marker);
  }else{
   // Old eighteen-buffer recovery directories have no profile sidecar.
   fs::remove(root/"before-device-profile.txt");assert(RestoreSnapshot::read(root).model==DeviceModel::ti16);
  }
 }
 auto m=snapshot(DeviceModel::snow4).project.multi;auto normalized=m;normalized[10]^=1;multiChecksum(normalized);
 assert(sameMulti(normalized,m,DeviceModel::ti16));assert(!sameMulti(normalized,m,DeviceModel::snow4));
 std::cout<<"PASS four-/sixteen-part snapshots, six/eighteen buffers, model metadata, settling, ten recalls, strict Snow readback, inactive-part rejection and unchanged dump envelopes (offline only)\n";
}
