#pragma once
#include "virus/transport_core.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace virus::bridge {
// New host-side design, not a reconstruction of legacy shared-memory ABI.
// Clock units cannot be interchanged with USB frames or private 14-bit ticks.
enum class ClockDomain { plugin_sample_timeline };
struct AudioFrame {
    std::uint64_t generation = 0, host_frame = 0;
    std::array<std::int32_t,6> pcm{}; // left-justified signed PCM, three stereo pairs
};
struct MidiMessage {
    std::uint64_t generation = 0, block_start = 0;
    std::uint32_t sample_offset = 0;
    ClockDomain domain = ClockDomain::plugin_sample_timeline;
    std::array<std::uint8_t,3> bytes{};
};
template<class T, std::size_t Capacity> class Spsc {
    static_assert((Capacity & (Capacity-1)) == 0);
public:
    bool push(const T& value) noexcept {
        auto w=write_.load(std::memory_order_relaxed);
        if(w-read_.load(std::memory_order_acquire)==Capacity)return false;
        slots_[w&(Capacity-1)]=value;write_.store(w+1,std::memory_order_release);return true;
    }
    // Pointer belongs to consumer until pop(), producer cannot reuse its slot.
    const T* front() const noexcept {
        auto r=read_.load(std::memory_order_relaxed);
        return r==write_.load(std::memory_order_acquire)?nullptr:&slots_[r&(Capacity-1)];
    }
    void pop() noexcept {read_.fetch_add(1,std::memory_order_release);}
private:
    std::array<T,Capacity> slots_{};
    alignas(64) std::atomic<std::uint64_t> write_{0},read_{0};
};
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
struct Stats {std::uint64_t rendered=0,underrun=0,discarded_audio=0,midi_queued=0,midi_dropped=0,discontinuities=0;};
class DeviceBridge {
public:
    // One processor exclusively claims this bridge while inactive. The service
    // and bridge must outlive the processor. No allocation, USB or locks here.
    bool claim() noexcept {bool v=false;return claimed_.compare_exchange_strong(v,true);}
    void release() noexcept {claimed_.store(false);}
    std::uint64_t generation() const noexcept {return generation_.load(std::memory_order_acquire);}
    bool ready() const noexcept {auto g=generation();return g && ready_.load(std::memory_order_acquire)==g;}
    // Processor-only lifecycle, serialized with its process callback by host.
    void restart(bool active) noexcept {
        ready_.store(0,std::memory_order_release);
        committedThrough_.store(0,std::memory_order_release);
        active_.store(false,std::memory_order_release);
        generation_.fetch_add(1,std::memory_order_acq_rel);
        active_.store(active,std::memory_order_release);++stats_.discontinuities;
    }
    void beginBlock() noexcept {if(disconnect_.exchange(false,std::memory_order_acq_rel))restart(active_.load());}
    // Service executor only. Readiness attests a caller-supplied sample clock
    // mapping and rate match; no code here infers either from USB completion time.
    bool serviceReady(std::uint64_t g) noexcept {
        if(!g||g!=generation()||!active_.load(std::memory_order_acquire)||blocked_.load()==g)return false;
        ready_.store(g,std::memory_order_release);return g==generation();
    }
    void serviceDisconnect() noexcept {
        blocked_.store(generation());ready_.store(0,std::memory_order_release);disconnect_.store(true,std::memory_order_release);
    }
    bool servicePublish(const AudioFrame& frame) noexcept {
        if(frame.generation!=generation()||!ready())return false;
        return audio_.push(frame);
    }
    // Connect the actual Engine capture queue to the host queue. The service
    // supplies the host frame of this decoded frame after a proven clock mapping.
    // Call only as capture queue's single consumer. Caller must discard/reset
    // old engine captures on generation changes; its queue has no timestamps.
    bool serviceTransferCapture(AudioFrameQueue& capture,std::uint64_t g,std::uint64_t hostFrame) noexcept {
        if(capture.channels()!=6||g!=generation()||!ready())return false;
        AudioFrame frame;frame.generation=g;frame.host_frame=hostFrame;
        if(capture.pop(frame.pcm.data(),1,6)!=QueueStatus::ok)return false;
        return servicePublish(frame); // On full bridge queue, explicit false; frame is dropped.
    }
    bool serviceTakeMidi(MidiMessage& message) noexcept {
        for(std::size_t i=0;i<2048;++i){auto* p=midi_.front();if(!p)return false;
            if(p->generation==generation()&&ready()&&p->block_start>=committedThrough_.load(std::memory_order_acquire))return false;
            message=*p;midi_.pop();if(ready()&&message.generation==generation())return true;
        }return false;
    }
    // Processor thread only. Bounded stale skipping; queued future frames remain.
    bool render(std::uint64_t hostFrame,std::array<std::int32_t,6>& out) noexcept {
        if(!ready())return false;
        const auto g=generation();
        for(std::size_t i=0;i<4096;++i){auto* p=audio_.front();if(!p)break;
            if(p->generation!=g||p->host_frame<hostFrame){audio_.pop();++stats_.discarded_audio;continue;}
            if(p->host_frame>hostFrame)break;
            out=p->pcm;audio_.pop();++stats_.rendered;return true;
        }++stats_.underrun;return false;
    }
    bool send(std::uint64_t blockStart,std::uint32_t offset,std::array<std::uint8_t,3> bytes) noexcept {
        MidiMessage m{generation(),blockStart,offset,ClockDomain::plugin_sample_timeline,bytes};
        if(!ready()||!midi_.push(m)){++stats_.midi_dropped;return false;}
        ++stats_.midi_queued;return true;
    }
    // Publish only after all MIDI for a block has been queued. Service sorts
    // messages by (block_start, sample_offset) before scheduling USB; queue order
    // is automation queues then input events and is deliberately not time order.
    void completeBlock(std::uint64_t end) noexcept {committedThrough_.store(end,std::memory_order_release);}
    // Stats read only when processor is quiescent. Service does not mutate stats.
    const Stats& stats() const noexcept {return stats_;}
private:
    Spsc<AudioFrame,4096> audio_;
    Spsc<MidiMessage,2048> midi_;
    std::atomic<std::uint64_t> generation_{0},ready_{0},blocked_{0},committedThrough_{0};
    std::atomic<bool> active_{false},disconnect_{false},claimed_{false};
    Stats stats_{};
};
} // namespace virus::bridge
