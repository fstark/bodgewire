#pragma once

#include <string>

#include "sim/component.hpp"

namespace bw {

// BCD to 7-segment decoder, outputs high = lit (2 kΩ pull-ups inside). The 74LS47's pinout,
// function table and glyphs: 1 B, 2 C, 3 LT_N, 4 BI_N (blanking input only), 5 RBI_N, 6 D, 7 A,
// 8 GND, 9 SE, 10 SD, 11 SC, 12 SB, 13 SA, 14 SG, 15 SF, 16 VCC.
class Ls48 final : public Component {
public:
    explicit Ls48(std::string reference);
    static constexpr PinIndex kB = 0, kC = 1, kLtN = 2, kBiN = 3, kRbiN = 4, kD = 5, kA = 6,
                              kSE = 8, kSD = 9, kSC = 10, kSB = 11, kSA = 12, kSG = 13, kSF = 14;

    void evaluate(EvalContext& ctx) override;
};

}  // namespace bw
