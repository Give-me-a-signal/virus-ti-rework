#pragma once
#include "pluginterfaces/base/funknown.h"

namespace virus::bridge {
class DeviceBridge;

// Private, in-process development-host interface. Host and plugin must use the
// same DeviceBridge header/ABI. This pointer cannot cross a process boundary.
// Query this on the processor IComponent, not the edit controller. Querying adds
// a COM reference; release that reference before unloading the plugin module.
class IVirusDeviceBridge : public Steinberg::FUnknown {
public:
    inline static const Steinberg::FUID iid{
        0xE44D27A1, 0x23CB4BF2, 0x9B12A96C, 0x5CB81031};

    // Host lifecycle thread only, serialized with activation and processing.
    // Attach while inactive and not processing; the caller retains ownership.
    // The bridge must live until detached or the processor is destroyed.
    // Pass nullptr to detach back to the processor's silent internal bridge.
    // Attaching does not open hardware, start the service, or assert readiness.
    virtual Steinberg::tresult PLUGIN_API attachDeviceBridge(DeviceBridge*) = 0;
};
} // namespace virus::bridge
