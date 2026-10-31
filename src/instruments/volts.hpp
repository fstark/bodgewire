#pragma once

#include "sim/recorder.hpp"

namespace bw {

inline constexpr double kLowVolts = 0.2, kHighVolts = 3.5, kFloatingVolts = 1.4,
                        kUnknownVolts = 1.0;

// Voltage of a logic Net (not a rail); power off is the caller's 0 V.
constexpr double LogicVolts(Sample sample) {
    if (sample.floating) {
        return kFloatingVolts;
    }
    switch (sample.value) {
    case LogicValue::Zero:
        return kLowVolts;
    case LogicValue::One:
        return kHighVolts;
    default:
        return kUnknownVolts;
    }
}

}  // namespace bw
