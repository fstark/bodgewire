#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

// 8-bit serial-in, parallel-out shift register, asynchronous clear. Pins per datasheet: 1 A,
// 2 B, 3 QA, 4 QB, 5 QC, 6 QD, 7 GND, 8 CLK, 9 CLR_N, 10 QE, 11 QF, 12 QG, 13 QH, 14 VCC.
class Ls164 final : public Component {
public:
    explicit Ls164(std::string reference);
    static constexpr PinIndex kA = 0, kB = 1, kQA = 2, kQB = 3, kQC = 4, kQD = 5, kClk = 7,
                              kClrN = 8, kQE = 9, kQF = 10, kQG = 11, kQH = 12;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    std::array<LogicValue, 8> stages_{};  // QA..QH; X after an unknown clear
};

}  // namespace bw
