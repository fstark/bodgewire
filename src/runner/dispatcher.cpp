#include "runner/dispatcher.hpp"

#include <algorithm>
#include <functional>
#include <utility>

namespace bw {

Dispatcher::Dispatcher(std::vector<Consumer*> consumers) : consumers_(std::move(consumers)) {}

void Dispatcher::stage(std::uint8_t slot, Sample sample) { staged_[slot].push_back(sample); }

void Dispatcher::apply(const Notice& notice) {
    switch (notice.kind) {
    case NoticeKind::Watch:
        slotNet_[notice.slot] = notice.net;
        staged_[notice.slot].clear();
        last_[notice.slot].reset();
        active_[notice.slot] = true;
        break;
    case NoticeKind::Rewatch:
        if (active_[notice.slot] && last_[notice.slot]) {
            for (Consumer* consumer : consumers_) {
                const auto nets = consumer->nets();
                if (std::ranges::find(nets, slotNet_[notice.slot]) != nets.end()) {
                    consumer->seed(slotNet_[notice.slot], *last_[notice.slot]);
                }
            }
        }
        break;
    case NoticeKind::Unwatch:
        active_[notice.slot] = false;
        staged_[notice.slot].clear();
        break;
    case NoticeKind::PowerOn:  // nothing stale is staged: the Runner waits for us first
        last_.fill(std::nullopt);
        for (Consumer* consumer : consumers_) {
            consumer->powerOn();
        }
        break;
    case NoticeKind::PowerOff:
        for (Consumer* consumer : consumers_) {
            consumer->powerOff();
        }
        break;
    case NoticeKind::SliceEnd:
        sliceEnd(notice.time);
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
