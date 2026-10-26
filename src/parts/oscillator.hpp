#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

// "14.31818MHz" → period in ps, rounded to nearest; error text if malformed or ≤ 0.
std::expected<Time, std::string> PeriodFromFrequency(std::string_view text);

// Crystal oscillator can, DIP-14 footprint: 7 GND, 8 OUT, 14 VCC, other positions NC<n>.
class Oscillator final : public Component {
public:
    Oscillator(std::string reference, Time period);  // Check(period >= 2)
    static constexpr PinIndex kOut = 7;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    Time highTime_;
    Time lowTime_;
    LogicValue level_ = LogicValue::Zero;
};

}  // namespace bw
