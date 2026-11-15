#pragma once

#include <string>

#include "sim/component.hpp"

namespace bw {

// Quad IEEE-488 bus transceiver (Motorola MC3446A), logic only. Pins per datasheet: 1 RA, 2 BUSA,
// 3 DA, 4 ENABC_N, 5 DB, 6 BUSB, 7 RB, 8 GND, 9 RC, 10 BUSC, 11 DC, 12 END_N, 13 DD, 14 BUSD,
// 15 RD, 16 VCC. A channel's bus pin is pulled low while its driver input D and its enable are
// both low, else released to the chip's own termination (a PullUp Pin: its 1 is weak); its
// receiver output R follows the bus. Non-inverting both ways. ENABC_N enables channels A-C,
// END_N channel D.
class Mc3446 final : public Component {
public:
    explicit Mc3446(std::string reference);
    static constexpr PinIndex kEnAbc = 3, kEnD = 11;

    void evaluate(EvalContext& ctx) override;
};

}  // namespace bw
