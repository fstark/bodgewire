#pragma once

#include <cstdint>
#include <vector>

#include "sim/logic.hpp"
#include "sim/time.hpp"

namespace bw {

inline constexpr std::uint32_t kGenerationMask = (1u << 29) - 1;

// generation << 3 | isWake << 2 | LogicValue
constexpr std::uint32_t MakeStamp(std::uint32_t generation, bool isWake, LogicValue value) {
    return (generation & kGenerationMask) << 3 | static_cast<std::uint32_t>(isWake) << 2 |
           static_cast<std::uint32_t>(value);
}
constexpr std::uint32_t StampGeneration(std::uint32_t stamp) { return stamp >> 3; }
constexpr bool StampIsWake(std::uint32_t stamp) { return (stamp & 4u) != 0; }
constexpr LogicValue StampValue(std::uint32_t stamp) {
    return static_cast<LogicValue>(stamp & 3u);
}

// Ordered by (time, target, stamp): same-time pop order is identical on every platform (S4).
struct Event {
    Time time;
    std::uint32_t target;  // PinId, or ComponentId for a wake
    std::uint32_t stamp;
};
static_assert(sizeof(Event) == 16);

// Min-heap backed by a vector: no allocation per event once grown.
class EventQueue {
public:
    void push(Event event);
    const Event& top() const { return heap_.front(); }
    void pop();
    bool empty() const { return heap_.empty(); }
    std::size_t size() const { return heap_.size(); }
    void clear() { heap_.clear(); }

private:
    std::vector<Event> heap_;
};

}  // namespace bw
