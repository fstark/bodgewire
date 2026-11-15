#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Hex bus buffer, tri-state, two active-low enables shared by all six (the ZX80's keyboard
// buffer). Pins per datasheet: 1 G1_N, 2 A1, 3 Y1, 4 A2, 5 Y2, 6 A3, 7 Y3, 8 GND, 9 Y4, 10 A4,
// 11 Y5, 12 A5, 13 Y6, 14 A6, 15 G2_N, 16 VCC. Each Y follows its A while G1_N and G2_N are both
// low, else Z.
class Ls365 final : public Component {
public:
    explicit Ls365(std::string reference);
    static constexpr PinIndex kG1 = 0, kG2 = 14;
    static constexpr std::array<PinIndex, 6> kA{1, 3, 5, 9, 11, 13};
    static constexpr std::array<PinIndex, 6> kY{2, 4, 6, 8, 10, 12};

    void powerOn(PowerOnContext& ctx) override;  // outputs released
    void evaluate(EvalContext& ctx) override;

private:
    // Last value scheduled per buffer: picks the data, enable or disable delay (as the 74LS244).
    std::array<LogicValue, 6> driven_{};
};

}  // namespace bw
