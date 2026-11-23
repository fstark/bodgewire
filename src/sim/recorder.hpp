#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "sim/ids.hpp"
#include "sim/logic.hpp"
#include "sim/time.hpp"

namespace bw {

struct Sample {
    Time time;
    LogicValue value;
    bool floating = false;  // no Driver: instruments only (logic is still `value`)
    friend bool operator==(const Sample&, const Sample&) = default;
};

// Value history of chosen Nets: initial value at start(), then every value or floating change.
class Recorder {
public:
    void start(NetId net, Sample initial);  // no-op if already recording
    void add(NetId net, Sample sample);     // only called for recorded Nets
    bool isRecording(NetId net) const {
        const std::uint32_t i = ToIndex(net);
        return i < slotOfNet_.size() && slotOfNet_[i] >= 0;
    }
    std::span<const NetId> nets() const {
        return nets_;
    }  // in start() order; stop() moves the last
    std::span<const Sample> samples(NetId net) const;
    void trim(NetId net);  // keeps only the last Sample; no-op if not recording
    void stop(NetId net);  // forgets the Net's Samples; no-op if not recording
    void clear();

    // The Samples of `nets` recorded after `after`, in time order (an earlier one in `nets` first
    // on a tie), each with its index in `nets`: how a caller that steps the Simulator itself feeds
    // several Nets to one instrument or device (bw run, the tests).
    std::vector<std::pair<std::size_t, Sample>> inOrder(std::span<const NetId> nets,
                                                        Time after) const;

private:
    std::vector<std::int32_t> slotOfNet_;  // -1 = not recording
    std::vector<std::vector<Sample>> slots_;
    std::vector<NetId> nets_;
};

}  // namespace bw
