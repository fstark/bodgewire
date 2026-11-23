#include "sim/recorder.hpp"

#include <algorithm>

#include <cassert>
#include <utility>

namespace bw {

void Recorder::start(NetId net, Sample initial) {
    if (isRecording(net)) {
        return;
    }
    const std::uint32_t i = ToIndex(net);
    if (i >= slotOfNet_.size()) {
        slotOfNet_.resize(i + 1, -1);
    }
    slotOfNet_[i] = static_cast<std::int32_t>(slots_.size());
    slots_.push_back({initial});
    nets_.push_back(net);
}

void Recorder::add(NetId net, Sample sample) {
    assert(isRecording(net));
    slots_[static_cast<std::size_t>(slotOfNet_[ToIndex(net)])].push_back(sample);
}

std::span<const Sample> Recorder::samples(NetId net) const {
    if (!isRecording(net)) {
        return {};
    }
    return slots_[static_cast<std::size_t>(slotOfNet_[ToIndex(net)])];
}

void Recorder::trim(NetId net) {
    if (!isRecording(net)) {
        return;
    }
    auto& samples = slots_[static_cast<std::size_t>(slotOfNet_[ToIndex(net)])];
    samples.erase(samples.begin(), samples.end() - 1);
}

void Recorder::stop(NetId net) {
    if (!isRecording(net)) {
        return;
    }
    const auto slot = static_cast<std::size_t>(slotOfNet_[ToIndex(net)]);
    if (slot + 1 != slots_.size()) {
        slots_[slot] = std::move(slots_.back());
        slotOfNet_[ToIndex(nets_.back())] = static_cast<std::int32_t>(slot);
        nets_[slot] = nets_.back();
    }
    slots_.pop_back();
    nets_.pop_back();
    slotOfNet_[ToIndex(net)] = -1;
}

void Recorder::clear() {
    slotOfNet_.clear();
    slots_.clear();
    nets_.clear();
}

std::vector<std::pair<std::size_t, Sample>> Recorder::inOrder(std::span<const NetId> nets,
                                                              Time after) const {
    std::vector<std::pair<std::size_t, Sample>> merged;
    for (std::size_t i = 0; i < nets.size(); ++i) {
        // From the back: after a trim only a few remain, and the new ones are at the end.
        const auto all = samples(nets[i]);
        std::size_t first = all.size();
        while (first > 0 && all[first - 1].time > after) {
            --first;
        }
        for (const Sample sample : all.subspan(first)) {
            merged.emplace_back(i, sample);
        }
    }
    std::ranges::stable_sort(merged, {}, [](const auto& entry) { return entry.second.time; });
    return merged;
}

}  // namespace bw
