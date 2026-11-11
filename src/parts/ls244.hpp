#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Octal buffer, tri-state, two groups of four. Pins per datasheet: 1 1OE_N, 2 1A1, 3 2Y4, 4 1A2,
// 5 2Y3, 6 1A3, 7 2Y2, 8 1A4, 9 2Y1, 10 GND, 11 2A1, 12 1Y4, 13 2A2, 14 1Y3, 15 2A3, 16 1Y2,
// 17 2A4, 18 1Y1, 19 2OE_N, 20 VCC. Each Y follows its A while its group's OE_N is low, else Z.
class Ls244 final : public Component {
public:
    explicit Ls244(std::string reference);
    static constexpr PinIndex k1OeN = 0, k2OeN = 18;

    void powerOn(PowerOnContext& ctx) override;  // outputs released
    void evaluate(EvalContext& ctx) override;

private:
    // Last value scheduled per buffer: picks the data, enable or disable delay. A pulse on OE_N
    // shorter than the delay is swallowed by the inertial drive, so this may name a value that
    // never showed; it only picks a delay a few ns off.
    std::array<LogicValue, 8> driven_{};
};

}  // namespace bw
