#pragma once

#include "sim/logic.hpp"
#include "sim/recorder.hpp"
#include "sim/time.hpp"

namespace bw {

inline constexpr Time kLedWindow = 20'000'000'000;  // ps: about what the eye integrates

// One LED as the eye sees it: share of each kLedWindow it was lit. Windows start at multiples
// of kLedWindow from power-on (t = 0). Time only moves forward.
class LedGlow {
public:
    void setAnode(Sample sample);
    void setCathode(Sample sample);
    void advanceTo(Time time);  // integrates, closes every window ending <= time
    float brightness() const { return brightness_; }  // last closed window; 0 before the first
    void reset();                                     // power-on: dark, both ends floating

private:
    bool lit() const;
    void integrateTo(Time time);

    Sample anode_{0, LogicValue::Z, true}, cathode_{0, LogicValue::Z, true};
    Time integrated_ = 0;  // lit time counted up to here
    Time windowEnd_ = kLedWindow;
    Time litTime_ = 0;  // ps lit in the current window
    float brightness_ = 0;
};

}  // namespace bw
