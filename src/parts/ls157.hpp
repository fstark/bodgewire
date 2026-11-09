#pragma once

#include <string>

#include "sim/component.hpp"

namespace bw {

// Quad 2-to-1 data selector. Pins per datasheet: 1 S, 2 1A, 3 1B, 4 1Y, 5 2A, 6 2B, 7 2Y, 8 GND,
// 9 3Y, 10 3B, 11 3A, 12 4Y, 13 4B, 14 4A, 15 G_N, 16 VCC. Y = A while S is low, B while S is
// high; G_N high forces every Y low.
class Ls157 final : public Component {
public:
    explicit Ls157(std::string reference);
    static constexpr PinIndex kS = 0, kGN = 14;

    void evaluate(EvalContext& ctx) override;
};

}  // namespace bw
