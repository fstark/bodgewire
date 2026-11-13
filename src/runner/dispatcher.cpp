#include "runner/dispatcher.hpp"

#include <algorithm>
#include <functional>
#include <utility>

namespace bw {

Dispatcher::Dispatcher(std::vector<Consumer*> consumers) : consumers_(std::move(consumers)) {}

void Dispatcher::stage(std::uint8_t slot, Sample sample) { staged_[slot].push_back(sample); }

void Dispatcher::apply(const Marker& marker) {
    switch (marker.kind) {
    case MarkerKind::Watch:
        slotNet_[marker.slot] = marker.net;
        staged_[marker.slot].clear();
        last_[marker.slot].reset();
        active_[marker.slot] = true;
        break;
    case MarkerKind::Rewatch:
        if (active_[marker.slot] && last_[marker.slot]) {
            for (Consumer* consumer : consumers_) {
                const auto nets = consumer->nets();
                if (std::ranges::find(nets, slotNet_[marker.slot]) != nets.end()) {
                    consumer->seed(slotNet_[marker.slot], *last_[marker.slot]);
                }
            }
        }
        break;
    case MarkerKind::Unwatch:
        active_[marker.slot] = false;
        staged_[marker.slot].clear();
        break;
    case MarkerKind::PowerOn:  // nothing stale is staged: the Runner waits for us first
        last_.fill(std::nullopt);
        for (Consumer* consumer : consumers_) {
            consumer->powerOn();
        }
        break;
    case MarkerKind::PowerOff:
        for (Consumer* consumer : consumers_) {
            consumer->powerOff();
        }
        break;
    case MarkerKind::SliceEnd:
        sliceEnd(marker.time);
        break;
    }
}

// A heap merge over the slots holding Samples up to `end`: equal times go in slot order, as a
// scan of every slot would. Each slot's Consumers are found once per slice: a Consumer retargeted
// within the slice gets the old Net's last Samples of it (each such Consumer checks the Net).
void Dispatcher::sliceEnd(Time end) {
    using Key = std::pair<Time, std::size_t>;  // next Sample's time, slot
    std::vector<Key>& heap = heap_;
    heap.clear();
    std::array<std::size_t, kMaxWatched> next{};
    for (std::size_t s = 0; s < kMaxWatched; ++s) {
        if (!active_[s] || staged_[s].empty() || staged_[s].front().time > end) {
            continue;
        }
        heap.emplace_back(staged_[s].front().time, s);
        auto& readers = readers_[s];
        readers.clear();
        for (Consumer* consumer : consumers_) {
            const auto nets = consumer->nets();
            if (std::ranges::find(nets, slotNet_[s]) != nets.end()) {
                readers.push_back(consumer);
            }
        }
    }
    std::ranges::make_heap(heap, std::greater<>{});
    while (!heap.empty()) {
        std::ranges::pop_heap(heap, std::greater<>{});
        const std::size_t slot = heap.back().second;
        const Sample sample = staged_[slot][next[slot]++];
        last_[slot] = sample;
        for (Consumer* consumer : readers_[slot]) {
            consumer->sample(slotNet_[slot], sample);
        }
        if (next[slot] < staged_[slot].size() && staged_[slot][next[slot]].time <= end) {
            heap.back().first = staged_[slot][next[slot]].time;
            std::ranges::push_heap(heap, std::greater<>{});
        } else {
            heap.pop_back();
        }
    }
    for (std::size_t s = 0; s < kMaxWatched; ++s) {
        staged_[s].erase(staged_[s].begin(),
                         staged_[s].begin() + static_cast<std::ptrdiff_t>(next[s]));
    }
    for (Consumer* consumer : consumers_) {
        consumer->advanceTo(end);
    }
}

}  // namespace bw
