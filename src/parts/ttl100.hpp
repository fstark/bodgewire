#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "sim/component.hpp"

namespace bw {

// 8-bit bistable latch, two enables of 4 bits (the PET 2001's row-start latch). Pins per datasheet
// (TI76 p. 7-113): 1 NC1, 2 1D1, 3 1D2, 4 1Q2, 5 1Q1, 6 NC6, 7 GND, 8 2Q1, 9 2Q2, 10 2D2, 11 2D1,
// 12 2G, 13 NC13, 14 NC14, 15 2D3, 16 2D4, 17 2Q4, 18 2Q3, 19 1Q3, 20 1Q4, 21 1D4, 22 1D3, 23 1G,
// 24 VCC. While nG is high its four Q follow their D; when it falls they hold the D from just
// before.
class Ttl100 final : public Component {
public:
    explicit Ttl100(std::string reference);
    static constexpr std::array<PinIndex, 2> kEnable{22, 11};  // 1G, 2G
    static constexpr std::array<std::array<PinIndex, 4>, 2> kD{{{1, 2, 21, 20}, {10, 9, 14, 15}}};
    static constexpr std::array<std::array<PinIndex, 4>, 2> kQ{{{4, 3, 18, 19}, {7, 8, 17, 16}}};

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    std::array<std::array<LogicValue, 4>, 2> q_{};
};

}  // namespace bw
