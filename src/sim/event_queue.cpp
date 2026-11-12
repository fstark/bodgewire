#include "sim/event_queue.hpp"

#include <algorithm>
#include <cassert>

namespace bw {

namespace {

// (time, target, stamp) as one number: times are never negative, so its order is theirs.
unsigned __int128 Key(const Event& e) {
    return static_cast<unsigned __int128>(static_cast<std::uint64_t>(e.time)) << 64 |
           static_cast<std::uint64_t>(e.target) << 32 | e.stamp;
}

// std heaps are max-heaps: "later" compares less so the earliest event is on top.
bool Later(const Event& a, const Event& b) { return Key(a) > Key(b); }

}  // namespace

void EventQueue::push(Event event) {
    assert(event.time >= 0);
    heap_.push_back(event);
    std::push_heap(heap_.begin(), heap_.end(), Later);
}

void EventQueue::pop() {
    assert(!heap_.empty());
    std::pop_heap(heap_.begin(), heap_.end(), Later);
    heap_.pop_back();
}

}  // namespace bw
