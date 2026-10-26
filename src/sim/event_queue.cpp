#include "sim/event_queue.hpp"

#include <algorithm>
#include <cassert>

namespace bw {

namespace {

// std heaps are max-heaps: "later" compares less so the earliest event is on top.
bool Later(const Event& a, const Event& b) {
    if (a.time != b.time) {
        return a.time > b.time;
    }
    if (a.target != b.target) {
        return a.target > b.target;
    }
    return a.stamp > b.stamp;
}

}  // namespace

void EventQueue::push(Event event) {
    heap_.push_back(event);
    std::push_heap(heap_.begin(), heap_.end(), Later);
}

void EventQueue::pop() {
    assert(!heap_.empty());
    std::pop_heap(heap_.begin(), heap_.end(), Later);
    heap_.pop_back();
}

}  // namespace bw
