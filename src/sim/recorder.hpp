#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "sim/ids.hpp"
#include "sim/logic.hpp"
#include "sim/time.hpp"

namespace bw {

struct Sample {
    Time time;
    LogicValue value;
    friend bool operator==(const Sample&, const Sample&) = default;
};

// Value history of chosen Nets: initial value at start(), then every change.
class Recorder {
public:
    void start(NetId net, Time now, LogicValue value);  // no-op if already recording
    void add(NetId net, Time time, LogicValue value);   // only called for recorded Nets
    bool isRecording(NetId net) const {
        const std::uint32_t i = ToIndex(net);
        return i < slotOfNet_.size() && slotOfNet_[i] >= 0;
    }
    std::span<const NetId> nets() const { return nets_; }  // in start() order
    std::span<const Sample> samples(NetId net) const;
    void clear();

private:
    std::vector<std::int32_t> slotOfNet_;  // -1 = not recording
    std::vector<std::vector<Sample>> slots_;
    std::vector<NetId> nets_;
};

}  // namespace bw
