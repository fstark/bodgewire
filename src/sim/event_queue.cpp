#include "sim/event_queue.hpp"

#include <algorithm>
#include <cassert>

namespace bw {

namespace {

constexpr std::size_t kFirstRuns = 64, kFirstKeys = 1024;  // doubled when full of pending events

// Insertion sort: takes are short (1 or 2 events, 8 or 16 for a bus) and nearly sorted.
void Sort(std::span<EventKey> keys) {
    for (std::size_t i = 1; i < keys.size(); ++i) {
        const EventKey key = keys[i];
        std::size_t j = i;
        for (; j > 0 && keys[j - 1] > key; --j) {
            keys[j] = keys[j - 1];
        }
        keys[j] = key;
    }
}

}  // namespace

EventQueue::EventQueue() : runs_(kFirstRuns), keys_(kFirstKeys) { runs_[0].time = kNever; }

void EventQueue::openRun(Time time) {
    assert(time >= 0 && time < kNever);
    if (openTime_ >= 0) {
        runs_[open_].end = static_cast<std::uint32_t>(size_);
    }
    if (tail_ + 2 > runs_.size()) {  // no room for one more run and the kNever after it
        if (head_ > 0) {             // taken runs first: move the pending ones down
            std::copy(runs_.begin() + static_cast<std::ptrdiff_t>(head_),
                      runs_.begin() + static_cast<std::ptrdiff_t>(tail_), runs_.begin());
            tail_ -= head_;
            head_ = 0;
        } else {
            runs_.resize(runs_.size() * 2);
        }
    }
    // Later runs move up one place; a run of the same time stays before the new one.
    std::size_t at = tail_++;
    runs_[tail_].time = kNever;
    for (; at > head_ && runs_[at - 1].time > time; --at) {
        runs_[at] = runs_[at - 1];
    }
    runs_[at] = {time, static_cast<std::uint32_t>(size_), 0};
    open_ = at;
    openTime_ = time;
}

std::span<const EventKey> EventQueue::takeNext() {
    assert(!empty());
    if (openTime_ >= 0) {
        runs_[open_].end = static_cast<std::uint32_t>(size_);
    }
    const Run first = runs_[head_++];
    if (first.time == openTime_) {
        openTime_ = -1;  // the open run is one of those taken
    }
    std::span<EventKey> events(keys_.data() + first.begin, first.end - first.begin);
    if (runs_[head_].time == first.time) {  // more runs of this time: gather them
        batch_.assign(events.begin(), events.end());
        for (; runs_[head_].time == first.time; ++head_) {
            batch_.insert(batch_.end(), keys_.begin() + runs_[head_].begin,
                          keys_.begin() + runs_[head_].end);
        }
        events = batch_;
    }
    if (head_ == tail_) {  // nothing pending: start again from the front
        head_ = tail_ = 0;
        runs_[0].time = kNever;
        size_ = 0;
    }
    Sort(events);
    return events;
}

// keys_ is full: moves the pending runs to the front of the other buffer, the open run last so
// that it can still grow, and doubles the buffer if they fill half of it.
void EventQueue::makeRoom() {
    const bool hasOpen = openTime_ >= 0;
    if (hasOpen) {
        runs_[open_].end = static_cast<std::uint32_t>(size_);
    }
    std::size_t pending = 0;
    for (std::size_t i = head_; i < tail_; ++i) {
        pending += runs_[i].end - runs_[i].begin;
    }
    spare_.resize(pending * 2 > keys_.size() ? keys_.size() * 2 : keys_.size());
    std::size_t at = 0;
    const auto move = [&](Run& run) {
        std::copy(keys_.begin() + run.begin, keys_.begin() + run.end,
                  spare_.begin() + static_cast<std::ptrdiff_t>(at));
        const std::size_t count = run.end - run.begin;
        run.begin = static_cast<std::uint32_t>(at);
        run.end = static_cast<std::uint32_t>(at + count);
        at += count;
    };
    for (std::size_t i = head_; i < tail_; ++i) {
        if (!hasOpen || i != open_) {
            move(runs_[i]);
        }
    }
    if (hasOpen) {
        move(runs_[open_]);
    }
    keys_.swap(spare_);
    size_ = at;
}

void EventQueue::clear() {
    head_ = tail_ = 0;
    runs_[0].time = kNever;
    size_ = 0;
    openTime_ = -1;
}

}  // namespace bw
