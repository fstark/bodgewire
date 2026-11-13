#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Octal D flip-flop, common clock (rising edge) and clear. Pins per datasheet: 1 CLR_N, 2 1Q,
// 3 1D, 4 2D, 5 2Q, 6 3Q, 7 3D, 8 4D, 9 4Q, 10 GND, 11 CLK, 12 5Q, 13 5D, 14 6D, 15 6Q, 16 7Q,
// 17 7D, 18 8D, 19 8Q, 20 VCC.
class Ls273 final : public Component {
public:
    explicit Ls273(std::string reference);
    static constexpr PinIndex kClrN = 0, kClk = 10;

    void powerOn(PowerOnContext& ctx) override;  // each state random
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 8> state_{};  // Zero, One or X
};

}  // namespace bw
