#pragma once

#include <string>

#include "sim/component.hpp"

namespace bw {

// 3-to-8 line decoder/demultiplexer. Pins per datasheet: 1 A, 2 B, 3 C, 4 G2A_N, 5 G2B_N, 6 G1,
// 7 Y7_N, 8 GND, 9 Y6_N, 10 Y5_N, 11 Y4_N, 12 Y3_N, 13 Y2_N, 14 Y1_N, 15 Y0_N, 16 VCC. While G1 is
// high and G2A_N, G2B_N are low, the output numbered by C B A is low; otherwise all are high.
class Ls138 final : public Component {
public:
    explicit Ls138(std::string reference);
    static constexpr PinIndex kA = 0, kB = 1, kC = 2, kG2aN = 3, kG2bN = 4, kG1 = 5;

    void evaluate(EvalContext& ctx) override;
};

}  // namespace bw
