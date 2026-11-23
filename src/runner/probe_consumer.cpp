#include "runner/probe_consumer.hpp"

#include <cstdint>
#include <optional>

namespace bw {

namespace {

constexpr std::uint64_t kHigh = 1, kLow = 2, kPulse = 4, kValid = 8;

}  // namespace

ProbeLamps ProbeConsumer::lamps() const {
    const std::optional<NetId> net = touched();
    const std::uint64_t p = published_.load(std::memory_order_acquire);
    if (!net || (p & kValid) == 0 || (p >> 32) != ToIndex(*net)) {
        return {};
    }
    return {.high = (p & kHigh) != 0, .low = (p & kLow) != 0, .pulse = (p & kPulse) != 0};
}

void ProbeConsumer::advanceTo(Time time) {
    const std::int64_t t = follow();
    if (t < 0) {
        published_.store(0, std::memory_order_release);
        return;
    }
    const ProbeLamps lamps = state_.lamps(time);
    published_.store(static_cast<std::uint64_t>(t) << 32 | kValid | (lamps.high ? kHigh : 0) |
                         (lamps.low ? kLow : 0) | (lamps.pulse ? kPulse : 0),
                     std::memory_order_release);
}

}  // namespace bw
