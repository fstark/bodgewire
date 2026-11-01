#pragma once

#include <string>

#include "sim/component.hpp"

namespace bw {

// Red 5 mm LED. Pin index = KiCad pad − 1: pad 1 is the cathode (LED_THT convention).
// Passive: both Pins are loads that never pull a floating Net (Family::Mos), no behaviour.
class Led final : public Component {
public:
    explicit Led(std::string reference);
    void evaluate(EvalContext& ctx) override;
};

}  // namespace bw
