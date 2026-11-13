#pragma once

#include <string>

#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

// Power-on reset, logic only: stands in for the resistor, capacitor and Schmitt trigger that hold
// a processor in reset while the supply settles. DIP-8: 1 RST_N (open collector), 4 GND, 8 VCC,
// the others NC. RST_N pulls low from power-on for `hold`, then lets go for good.
class PowerOnReset final : public Component {
public:
    PowerOnReset(std::string reference, Time hold);  // Check(hold >= 1)
    static constexpr PinIndex kRstN = 0;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    Time hold_;
};

}  // namespace bw
