#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "runner/consumer.hpp"
#include "sim/ids.hpp"
#include "sim/recorder.hpp"
#include "sim/time.hpp"

namespace bw {

// Ring slots: every Clip, Hook, lead, the probe and a Machine's lamps (64 + 4 + 2 + 1 + up to
// 32 LED Nets on the LED Machines, with room).
inline constexpr std::size_t kMaxWatched = 128;

enum class MarkerKind : std::uint8_t { PowerOn, SliceEnd, Watch, Rewatch, Unwatch, PowerOff };

// Control marker, simulation → devices thread, in order with the Samples.
struct Marker {
    MarkerKind kind;
    std::uint8_t slot = 0;  // Watch, Rewatch, Unwatch
    NetId net{};            // Watch
    Time time = 0;          // SliceEnd: slice end
};

// Devices-side staging, time merge and fan-out to Consumers. Single-threaded.
class Dispatcher {
public:
    explicit Dispatcher(std::vector<Consumer*> consumers);
    void stage(std::uint8_t slot, Sample sample);  // drained from a ring
    void apply(const Marker& marker);
    bool isActive(std::uint8_t slot) const { return active_[slot]; }

private:
    void sliceEnd(Time end);

    std::vector<Consumer*> consumers_;
    std::array<NetId, kMaxWatched> slotNet_{};
    std::array<std::vector<Sample>, kMaxWatched> staged_;  // drained, not yet dispatched
    std::array<std::optional<Sample>, kMaxWatched> last_;  // last dispatched, for Rewatch
    std::array<bool, kMaxWatched> active_{};
    std::array<std::vector<Consumer*>, kMaxWatched> readers_;  // per slot, for this slice
    std::vector<std::pair<Time, std::size_t>> heap_;           // sliceEnd's merge
};

}  // namespace bw
