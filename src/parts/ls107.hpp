#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Dual JK flip-flop, clear, negative-edge clocked. Pins per datasheet: 1 1J, 2 1Q_N, 3 1Q, 4 1K,
// 5 2Q, 6 2Q_N, 7 GND, 8 2J, 9 2CLK, 10 2CLR_N, 11 2K, 12 1CLK, 13 1CLR_N, 14 VCC.
class Ls107 final : public Component {
public:
    explicit Ls107(std::string reference);
    void powerOn(PowerOnContext& ctx) override;  // each state random
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 2> state_{};  // Zero, One or X
};

}  // namespace bw
