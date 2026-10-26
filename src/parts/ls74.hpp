#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Dual D flip-flop, preset and clear. Pins per datasheet: 1 1CLR_N, 2 1D, 3 1CLK, 4 1PRE_N,
// 5 1Q, 6 1Q_N, 7 GND, 8 2Q_N, 9 2Q, 10 2PRE_N, 11 2CLK, 12 2D, 13 2CLR_N, 14 VCC.
class Ls74 final : public Component {
public:
    explicit Ls74(std::string reference);
    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 2> state_{};
};

}  // namespace bw
