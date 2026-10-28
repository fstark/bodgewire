#pragma once

#include <string>

#include "sim/simulator.hpp"

namespace bw {

struct ProbeLamps {
    bool high = false, low = false, pulse = false;
};

// The P2d lamp model over one Net's Samples: the first Sample is the initial state.
class ProbeState {
public:
    static constexpr Time kPulseStretch = 100'000'000'000;  // ps, 100 ms

    void reset();              // no Sample yet: all lamps dark
    void feed(Sample sample);  // in time order
    ProbeLamps lamps(Time now) const;

private:
    bool started_ = false;
    LogicValue value_ = LogicValue::X;
    bool floating_ = true;
    Time lastChange_ = -1;  // last value change after the first Sample; -1 = none
};

// TTL logic probe on one Net. Construct after powerOn() (it clears recordings).
class LogicProbe {
public:
    LogicProbe(Simulator& sim, NetId net);  // starts recording the Net
    void update();                          // reads changes recorded since the last update
    ProbeLamps lamps() const;               // at sim.now()

private:
    Simulator& sim_;
    NetId net_;
    Time fedUntil_ = -1;  // Samples up to here already read
    ProbeState state_;
};

std::string FormatLamps(ProbeLamps lamps);  // "HIGH", "LOW PULSE", "-"

}  // namespace bw
