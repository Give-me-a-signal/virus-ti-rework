#pragma once
#include "parameter_registry.h"
#include "hardware_service.h"
#include "virus/device_bridge.hpp"
#include "virus/vst_bridge_interface.hpp"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "base/source/timer.h"
#include <atomic>
#include <array>
namespace VirusRework {
using namespace Steinberg;
using namespace Steinberg::Vst;
inline const FUID kProcessorID(0x5695B49E,0x529D4E17,0xA2855601,0xA10B277E);
inline const FUID kControllerID(0x5BCB58FD,0xB9C442DE,0x9699904C,0x625DCCB1);
class Processor final : public AudioEffect, public virus::bridge::IVirusDeviceBridge {
public:
 Processor();
 ~Processor() override;
 // Attach only while inactive; caller-owned bridge outlives processor.
 bool attachBridge(virus::bridge::DeviceBridge&) noexcept;
 tresult PLUGIN_API attachDeviceBridge(virus::bridge::DeviceBridge*) override;
 tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
  if(!obj)return kInvalidArgument;
  QUERY_INTERFACE(iid,obj,virus::bridge::IVirusDeviceBridge::iid,virus::bridge::IVirusDeviceBridge)
  return AudioEffect::queryInterface(iid,obj);
 }
 DELEGATE_REFCOUNT(AudioEffect)
 tresult PLUGIN_API notify(IMessage*) override;
 tresult PLUGIN_API setupProcessing(ProcessSetup&) override;
 uint32 PLUGIN_API getLatencySamples() override;
 tresult PLUGIN_API setActive(TBool) override;
 tresult PLUGIN_API setProcessing(TBool) override;
 const virus::bridge::Stats& bridgeStats() const {return bridge_->stats();}
 static FUnknown* create(void*) { return static_cast<IAudioProcessor*>(new Processor); }
 tresult PLUGIN_API initialize(FUnknown*) override;
 tresult PLUGIN_API terminate() override;
 tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32,SpeakerArrangement*,int32) override;
 tresult PLUGIN_API canProcessSampleSize(int32) override;
 tresult PLUGIN_API process(ProcessData&) override;
 tresult PLUGIN_API setState(IBStream*) override;
 tresult PLUGIN_API getState(IBStream*) override;
 double value(size_t i) const { return values_[i].load(std::memory_order_relaxed); }
 uint64_t acceptedEvents() const {return events_;}
 uint64_t rejectedEvents() const {return rejected_;}
private:
 std::array<std::atomic<double>, kCatalog.size()> values_{};
 uint64_t events_=0,rejected_=0;
 virus::bridge::DeviceBridge disconnected_;
 virus::bridge::DeviceBridge* bridge_=&disconnected_;
 std::unique_ptr<HardwareService> hardware_;
 bool active_=false,processing_=false,projectValid_=false;
 uint64_t hostFrame_=0;
 int64_t expectedProject_=0;
};
class Controller final : public EditControllerEx1, public IMidiMapping, public ITimerCallback {
public:
 ~Controller() override;
 tresult PLUGIN_API terminate() override;
 void onTimer(Timer*) override;
 void hardwareCommand(int);
 void hardwarePreset(const std::vector<uint8_t>&,unsigned part=0);
 bool hardwareConnected=false;
 unsigned hardwarePartCount=16;
 MeterSnapshot hardwareMeters;
 ProjectPatches hardwarePatches;
 std::vector<uint8_t> hardwarePatch;
 uint64_t patchRevision=0;
 tresult PLUGIN_API notify(IMessage*) override;
 std::string hardwareText="Disconnected · Start with TI in Single mode";
 static FUnknown* create(void*) {return static_cast<IEditController*>(new Controller);}
 tresult PLUGIN_API initialize(FUnknown*) override;
 tresult PLUGIN_API setComponentState(IBStream*) override;
 IPlugView* PLUGIN_API createView(FIDString) override;
 tresult PLUGIN_API getMidiControllerAssignment(int32,int16,CtrlNumber,ParamID&) override;
 tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
  QUERY_INTERFACE(iid,obj,IMidiMapping::iid,IMidiMapping)
  return EditControllerEx1::queryInterface(iid,obj);
 }
 DELEGATE_REFCOUNT(EditControllerEx1)
private:
 IPtr<Timer> stateTimer;
 int64 observedStateRevision=0;
};
}
