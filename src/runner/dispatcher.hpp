#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "runner/consumer.hpp"
#include "sim/ids.hpp"
#include "sim/recorder.hpp"
#include "sim/time.hpp"

namespace bw {

inline constexpr std::size_t kMaxWatched = 48;  // ring slots

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
};

}  // namespace bw
