#pragma once

#include <cstdint>
#include <span>
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
constexpr LogicValue StampValue(std::uint32_t stamp) { return static_cast<LogicValue>(stamp & 3u); }

// Ordered by (time, target, stamp): same-time order is identical on every platform (S4).
struct Event {
    Time time;
    std::uint32_t target;  // PinId, or ComponentId for a wake
    std::uint32_t stamp;
};
static_assert(sizeof(Event) == 16);

// An Event without its time: target << 32 | stamp, so integer order is (target, stamp).
using EventKey = std::uint64_t;
constexpr EventKey KeyOf(const Event& event) {
    return static_cast<EventKey>(event.target) << 32 | event.stamp;
}
constexpr std::uint32_t KeyTarget(EventKey key) { return static_cast<std::uint32_t>(key >> 32); }
constexpr std::uint32_t KeyStamp(EventKey key) { return static_cast<std::uint32_t>(key); }

// Pending events as runs. A run is the events of consecutive pushes for one time (a bus changing
// as a group), side by side in keys_: most pushes only append to the open run. The runs are few
// (about 5 on a bus), kept in time order, so a take is the first run, or the first few when they
// share a time. Opening a run is linear in the number of runs. No allocation once grown.
class EventQueue {
public:
    EventQueue();
    void push(Event event) {
        if (event.time != openTime_) {
            openRun(event.time);
        }
        if (size_ == keys_.size()) {
            makeRoom();
        }
        keys_[size_++] = KeyOf(event);
    }
    bool empty() const { return head_ == tail_; }
    Time nextTime() const { return runs_[head_].time; }  // earliest pending time; kNever if empty
    // Removes every event of nextTime() and gives them sorted by (target, stamp); valid until the
    // next push or take.
    std::span<const EventKey> takeNext();
    void clear();

    static constexpr Time kNever = INT64_MAX;  // no event has this time

private:
    struct Run {
        Time time;
        std::uint32_t begin, end;  // in keys_; the open run ends at size_
    };
    void openRun(Time time);
    void makeRoom();

    // Pending runs: [head_, tail_), by time, runs of one time in push order. runs_[tail_].time is
    // kNever, so a scan forward needs no bound.
    std::vector<Run> runs_;
    std::size_t head_ = 0, tail_ = 0;
    std::vector<EventKey> keys_;  // [0, size_): the runs' events, and holes left by taken runs
    std::size_t size_ = 0;
    std::vector<EventKey> spare_;  // makeRoom()'s other buffer
    std::vector<EventKey> batch_;  // a take made of several runs
    Time openTime_ = -1;           // the open run's time, -1 if none
    std::size_t open_ = 0;         // its place in runs_
};

}  // namespace bw
