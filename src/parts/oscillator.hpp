#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

// Crystal oscillator can, DIP-14 footprint: 7 GND, 8 OUT, 14 VCC, other positions NC<n>.
class Oscillator final : public Component {
public:
    Oscillator(std::string reference, Time period);  // Check(period >= 2)
    // Another period, as a Wrong Clock gives it (P8k): before powerOn. Check(period >= 2).
    void setPeriod(Time period);
    Time period() const { return highTime_ + lowTime_; }
    static constexpr PinIndex kOut = 7;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    Time highTime_;
    Time lowTime_;
    LogicValue level_ = LogicValue::Zero;
};

}  // namespace bw
