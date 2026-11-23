#pragma once

#include "sim/logic.hpp"
#include "sim/time.hpp"

namespace bw {

// A Pin's propagation delays (each Part's in parts/delays.hpp).
struct Delays {
    Time rise;  // tPLH
    Time fall;  // tPHL
};

// 1 → rise, 0 → fall, X/Z → the slower of the two.
constexpr Time DelayFor(Delays delays, LogicValue value) {
    if (value == LogicValue::One) {
        return delays.rise;
    }
    if (value == LogicValue::Zero) {
        return delays.fall;
    }
    return delays.rise > delays.fall ? delays.rise : delays.fall;
}

}  // namespace bw
