#pragma once

#include <optional>

#include "sim/simulator.hpp"

namespace bw {

inline constexpr Time kMeterWindow = 200'000'000'000;   // ps averaged per reading
inline constexpr Time kMeterRefresh = 250'000'000'000;  // ps between readings

// Time-weighted average of LogicVolts over the last kMeterWindow, refreshed at multiples of
// kMeterRefresh. The first Sample starts the averaging (window clipped to it).
class MeterState {
public:
    void reset();              // no Sample yet: no reading
    void feed(Sample sample);  // in time order
    void advanceTo(Time end);
    std::optional<double> reading() const { return reading_; }  // last refresh

private:
    void integrateTo(Time time);
    void refresh();

    bool started_ = false;
    double volts_ = 0;     // current LogicVolts
    Time integrated_ = 0;  // area_ covers [windowStart_, integrated_)
    Time windowStart_ = 0;
    Time next_ = 0;    // next refresh
    double area_ = 0;  // V·ps
    std::optional<double> reading_;
};

// Headless Multimeter (V) on one Net (tests). Construct after powerOn().
class Multimeter {
public:
    Multimeter(Simulator& sim, NetId net);  // starts recording the Net
    void update();                          // as LogicProbe::update
    std::optional<double> reading() const { return state_.reading(); }

private:
    Simulator& sim_;
    NetId net_;
    Time fedUntil_ = -1;
    MeterState state_;
};

}  // namespace bw
