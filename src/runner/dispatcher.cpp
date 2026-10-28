#include "runner/dispatcher.hpp"

#include <algorithm>
#include <utility>

namespace bw {

Dispatcher::Dispatcher(std::vector<Consumer*> consumers) : consumers_(std::move(consumers)) {}

void Dispatcher::stage(std::uint8_t slot, Sample sample) { staged_[slot].push_back(sample); }

void Dispatcher::apply(const Marker& marker) {
    switch (marker.kind) {
    case MarkerKind::Watch:
        slotNet_[marker.slot] = marker.net;
        staged_[marker.slot].clear();
        active_[marker.slot] = true;
        break;
    case MarkerKind::Unwatch:
        active_[marker.slot] = false;
        staged_[marker.slot].clear();
        break;
    case MarkerKind::PowerOn:
        for (auto& staged : staged_) {
            staged.clear();
        }
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

void Dispatcher::sliceEnd(Time end) {
    std::array<std::size_t, kMaxWatched> next{};
    while (true) {
        std::size_t best = kMaxWatched;
        for (std::size_t s = 0; s < kMaxWatched; ++s) {
            if (!active_[s] || next[s] == staged_[s].size() || staged_[s][next[s]].time > end) {
                continue;
            }
            if (best == kMaxWatched || staged_[s][next[s]].time < staged_[best][next[best]].time) {
                best = s;
            }
        }
        if (best == kMaxWatched) {
            break;
        }
        const Sample sample = staged_[best][next[best]++];
        for (Consumer* consumer : consumers_) {
            const auto nets = consumer->nets();
            if (std::ranges::find(nets, slotNet_[best]) != nets.end()) {
                consumer->sample(slotNet_[best], sample);
            }
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
