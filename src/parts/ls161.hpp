#pragma once

#include <cstdint>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Synchronous 4-bit binary counter, asynchronous clear. Pins per datasheet: 1 CLR_N, 2 CLK,
// 3 A, 4 B, 5 C, 6 D, 7 ENP, 8 GND, 9 LOAD_N, 10 ENT, 11 QD, 12 QC, 13 QB, 14 QA, 15 RCO, 16 VCC.
class Ls161 final : public Component {
public:
    explicit Ls161(std::string reference);
    static constexpr PinIndex kClrN = 0, kClk = 1, kA = 2, kB = 3, kC = 4, kD = 5, kEnp = 6,
                              kLoadN = 8, kEnt = 9, kQD = 10, kQC = 11, kQB = 12, kQA = 13,
                              kRco = 14;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    std::uint8_t count_ = 0;
    bool unknown_ = false;  // count is X
};

}  // namespace bw
