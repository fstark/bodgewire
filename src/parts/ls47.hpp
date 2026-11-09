#pragma once

#include <cstdint>
#include <string>

#include "sim/component.hpp"

namespace bw {

// BCD to 7-segment decoder/driver, open-collector outputs, low = lit. Pins per datasheet:
// 1 B, 2 C, 3 LT_N, 4 BI_N (BI/RBO: blanking input only), 5 RBI_N, 6 D, 7 A, 8 GND,
// 9 SE_N, 10 SD_N, 11 SC_N, 12 SB_N, 13 SA_N, 14 SG_N, 15 SF_N, 16 VCC.
class Ls47 final : public Component {
public:
    explicit Ls47(std::string reference);
    static constexpr PinIndex kB = 0, kC = 1, kLtN = 2, kBiN = 3, kRbiN = 4, kD = 5, kA = 6,
                              kSE = 8, kSD = 9, kSC = 10, kSB = 11, kSA = 12, kSG = 13, kSF = 14;

    void evaluate(EvalContext& ctx) override;
};

// Segments lit (bit 0 = a … bit 6 = g) for `value` 0..15 on D..A. Datasheet function table:
// BI_N low blanks and wins, then LT_N low lights all, then RBI_N low blanks a 0.
std::uint8_t Ls47Segments(unsigned value, bool lampTestN, bool rippleBlankingInN, bool blankingInN);

// Segments certainly lit and certainly dark (bit 0 = a … bit 6 = g) for the inputs `ctx` reads
// at A, B, C, D, LT_N, RBI_N, BI_N (the 74LS47 and 74LS48 pin indices): every value an X input
// could have is tried.
struct SegmentCertainty {
    std::uint8_t lit, dark;
};
SegmentCertainty ReadSegments(EvalContext& ctx);

}  // namespace bw
