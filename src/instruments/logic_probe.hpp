#pragma once

#include <string>

#include "sim/simulator.hpp"

namespace bw {

struct ProbeLamps {
    bool high = false, low = false, pulse = false;
};

// TTL logic probe on one Net. Construct after powerOn() (it clears recordings).
class LogicProbe {
public:
    static constexpr Time kPulseStretch = 100'000'000'000;  // ps, 100 ms

    LogicProbe(Simulator& sim, NetId net);  // starts recording the Net
    void update();                          // reads changes recorded since the last update
    ProbeLamps lamps() const;               // at sim.now()

private:
    Simulator& sim_;
    NetId net_;
    Time fedUntil_ = -1;    // Samples up to here already read
    Time lastChange_ = -1;  // last change after the recording started; -1 = none
    bool started_ = false;  // the first Sample is the initial value, not a change
};

std::string FormatLamps(ProbeLamps lamps);  // "HIGH", "LOW PULSE", "-"

}  // namespace bw
