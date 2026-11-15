#pragma once

#include <string>

#include "sim/component.hpp"

namespace bw {

// Dual 2-to-4 line decoder/demultiplexer. Pins per datasheet: 1 1G_N, 2 1A, 3 1B, 4-7
// 1Y0_N-1Y3_N, 8 GND, 9-12 2Y3_N-2Y0_N, 13 2B, 14 2A, 15 2G_N, 16 VCC. In each half, while G_N is
// low the output numbered by B A is low; otherwise all four are high.
class Ls139 final : public Component {
public:
    explicit Ls139(std::string reference);
    static constexpr PinIndex k1G = 0, k1A = 1, k1B = 2, k2B = 12, k2A = 13, k2G = 14;

    void evaluate(EvalContext& ctx) override;
};

}  // namespace bw
