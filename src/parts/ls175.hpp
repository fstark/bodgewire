#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Quad D flip-flop, common clock (rising edge) and clear. Pins per datasheet: 1 CLR_N, 2 1Q,
// 3 1Q_N, 4 1D, 5 2D, 6 2Q_N, 7 2Q, 8 GND, 9 CLK, 10 3Q, 11 3Q_N, 12 3D, 13 4D, 14 4Q_N, 15 4Q,
// 16 VCC.
class Ls175 final : public Component {
public:
    explicit Ls175(std::string reference);
    static constexpr PinIndex kClrN = 0, kClk = 8;

    void powerOn(PowerOnContext& ctx) override;  // each state random
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 4> state_{};  // Zero, One or X
};

}  // namespace bw
