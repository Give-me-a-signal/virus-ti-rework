#pragma once
#include <cstddef>
#include <cstdint>

namespace virus::midi {

// Offline snapshots only. This deliberately cannot attach to shared memory or
// advance the legacy read cursor. Words remain opaque, not USB MIDI CIN packets.
struct LegacyRingInfo {
    std::uint32_t read_index, write_index, capacity, queued_words;
};
enum class SnapshotResult { ok, invalid_size, invalid_header, output_too_small };

inline std::uint32_t snapshot_u32le(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
inline SnapshotResult inspect_legacy_ring_snapshot(const std::uint8_t* snapshot,
                                                    std::size_t bytes,
                                                    LegacyRingInfo& info) noexcept {
    if (!snapshot || bytes != 4096) return SnapshotResult::invalid_size;
    const auto read = snapshot_u32le(snapshot);
    const auto write = snapshot_u32le(snapshot + 4);
    const auto capacity = snapshot_u32le(snapshot + 8);
    if (capacity != 1021 || read >= capacity || write >= capacity)
        return SnapshotResult::invalid_header;
    info = {read, write, capacity, (write + capacity - read) % capacity};
    return SnapshotResult::ok;
}
inline SnapshotResult copy_legacy_ring_snapshot(const std::uint8_t* snapshot,
                                                 std::size_t bytes,
                                                 std::uint32_t* words,
                                                 std::size_t output_capacity,
                                                 LegacyRingInfo& info) noexcept {
    LegacyRingInfo candidate{};
    const auto result = inspect_legacy_ring_snapshot(snapshot, bytes, candidate);
    if (result != SnapshotResult::ok) return result;
    if (candidate.queued_words > output_capacity || (!words && candidate.queued_words))
        return SnapshotResult::output_too_small;
    for (std::uint32_t i = 0; i < candidate.queued_words; ++i) {
        const auto index = (candidate.read_index + i) % candidate.capacity;
        words[i] = snapshot_u32le(snapshot + 12 + index * 4);
    }
    info = candidate;
    return SnapshotResult::ok;
}
} // namespace virus::midi
