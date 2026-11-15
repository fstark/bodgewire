#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Octal transparent latch, tri-state outputs (the ZX80's RAM data latch). Pins per datasheet:
// 1 OE_N, 2 1Q, 3 1D, 4 2D, 5 2Q, 6 3Q, 7 3D, 8 4D, 9 4Q, 10 GND, 11 LE, 12 5Q, 13 5D, 14 6D,
// 15 6Q, 16 7Q, 17 7D, 18 8D, 19 8Q, 20 VCC. While LE is high each Q follows its D; when LE falls
// it holds the D from just before. OE_N high turns the outputs off, not the latch.
class Ls373 final : public Component {
public:
    explicit Ls373(std::string reference);
    static constexpr PinIndex kOe = 0, kLe = 10;
    static constexpr std::array<PinIndex, 8> kD{2, 3, 6, 7, 12, 13, 16, 17};
    static constexpr std::array<PinIndex, 8> kQ{1, 4, 5, 8, 11, 14, 15, 18};

    void powerOn(PowerOnContext& ctx) override;  // random latch, outputs per OE_N at the start
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 8> latch_{};
    std::array<LogicValue, 8> driven_{};  // last value scheduled per output
};

}  // namespace bw
