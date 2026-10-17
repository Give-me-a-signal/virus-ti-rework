#pragma once
#include "virus/device_bridge.hpp"
#include "project_patches.h"
#include "meters.h"
#include <memory>
#include <functional>
#include <string>
#include <vector>
namespace VirusRework {
// Owned by one processor. Saved connected states reconnect only during realtime processing.
class HardwareService {
public:
 explicit HardwareService(virus::bridge::DeviceBridge&);
 ~HardwareService();
 void shutdownControl(); // join API callbacks while the owning processor is still intact
 void shutdown(); // VST3 terminate: stop work and hand hardware restoration to the helper
 void command(int operation); // non-audio caller: 1 connect, 2 disconnect, 3 stock test tone, 8 host deactivation
 bool loadPreset(const std::vector<uint8_t>&,unsigned part=0); // selected RAM part
 std::vector<uint8_t> currentPreset(unsigned part=0) const;
 bool captureProject(ProjectPatches& patch, bool& reconnect); // non-audio, bounded worker read
 bool restoreProject(const ProjectPatches& patch, bool reconnect, const std::array<uint8_t,16>* volumes=nullptr);
 void stateCallbacks(std::function<std::vector<uint8_t>()> save,
                     std::function<bool(const std::vector<uint8_t>&)> restore);
 MeterSnapshot meters() const noexcept;
 std::string status() const;
 std::string controlEndpoint() const;
 void hostProcessing(bool active) noexcept;
 void setup(double rate,unsigned maxBlock=256); // inactive, non-audio caller
 void beginBlock(bool active,unsigned samples=0,double tempo=120,double beats=0,bool playing=false,bool beatsValid=false) noexcept;
 bool queueMidi(unsigned offset,std::array<uint8_t,3>)noexcept;
 void endBlock()noexcept;
 bool render(std::array<int32_t,6>&) noexcept;
 bool connected() const noexcept;
 uint64_t stateRevision() const noexcept;
 unsigned latency() const noexcept;
#ifdef VIRUS_REWORK_TEST
 static bool testCaptureUnderrunFallback(); // synthetic FIFO exhaustion; no device I/O
#endif
private:
 struct Impl;
 std::unique_ptr<Impl> impl;
};
}
