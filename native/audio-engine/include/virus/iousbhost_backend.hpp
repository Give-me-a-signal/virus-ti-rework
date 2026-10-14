#pragma once
#include "virus/audio_engine.hpp"
#include <memory>
#ifdef __OBJC__
@class IOUSBHostPipe;
namespace virus::audio {
// macOS 12+ adapter. Caller opens/selects the correct interfaces and pipes before
// construction, with BOTH interfaces using the SAME SERIAL dispatch queue. All
// Engine/Backend methods must execute on that queue; DAW threads use only queues.
// No discovery, seizure, alternate selection, rate change or firmware commands.
class IOUSBHostBackend final : public Backend {
public:
    IOUSBHostBackend(const Config&, IOUSBHostPipe* capture, IOUSBHostPipe* playback);
    ~IOUSBHostBackend();
    void bind(Engine&) noexcept; // Once, before start; engine must use this backend.
    SubmitStatus submit(const Request&) noexcept override;
    void cancel(Token) noexcept override;
    std::int32_t last_error() const noexcept;
    // FS: first_service is the nonzero absolute USB frame number.
    // HS: first_service is absolute USB frame number * 8 and packets/request=8.
    // Owner chooses a verified scheduling lead from frameNumberWithTime. Zero is
    // rejected to avoid conflating the engine timeline with OS ASAP semantics.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace virus::audio
#endif
