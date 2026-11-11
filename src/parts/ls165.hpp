#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

// 8-bit parallel-in, serial-out shift register. Pins per datasheet: 1 SH_LD_N, 2 CLK, 3 E, 4 F,
// 5 G, 6 H, 7 QH_N, 8 GND, 9 QH, 10 SER, 11 A, 12 B, 13 C, 14 D, 15 CLK_INH, 16 VCC.
// SH_LD_N low loads A-H at once; while high, each rise of CLK OR CLK_INH shifts SER -> QA ... QH.
class Ls165 final : public Component {
public:
    explicit Ls165(std::string reference);
    static constexpr PinIndex kShLdN = 0, kClk = 1, kE = 2, kF = 3, kG = 4, kH = 5, kQHN = 6,
                              kQH = 8, kSer = 9, kA = 10, kB = 11, kC = 12, kD = 13, kClkInh = 14;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 8> stages_{};  // QA..QH
};

}  // namespace bw
