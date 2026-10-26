#include "sim/recorder.hpp"

#include <cassert>

namespace bw {

void Recorder::start(NetId net, Time now, LogicValue value) {
    if (isRecording(net)) {
        return;
    }
    const std::uint32_t i = ToIndex(net);
    if (i >= slotOfNet_.size()) {
        slotOfNet_.resize(i + 1, -1);
    }
    slotOfNet_[i] = static_cast<std::int32_t>(slots_.size());
    slots_.push_back({{now, value}});
    nets_.push_back(net);
}

void Recorder::add(NetId net, Time time, LogicValue value) {
    assert(isRecording(net));
    slots_[static_cast<std::size_t>(slotOfNet_[ToIndex(net)])].push_back({time, value});
}

std::span<const Sample> Recorder::samples(NetId net) const {
    if (!isRecording(net)) {
        return {};
    }
    return slots_[static_cast<std::size_t>(slotOfNet_[ToIndex(net)])];
}

void Recorder::clear() {
    slotOfNet_.clear();
    slots_.clear();
    nets_.clear();
}

}  // namespace bw
