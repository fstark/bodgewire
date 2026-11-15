#pragma once

#include <cstdint>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Presettable binary counter/latch (the PET 2001's video address counters). Pins per datasheet
// (TI76 p. 7-259): 1 CT_LOAD_N, 2 QC, 3 C, 4 A, 5 QA, 6 CLK2, 7 GND, 8 CLK1, 9 QB, 10 B, 11 D, 12
// QD, 13 CLR_N, 14 VCC. QA toggles as CLK1 falls; QB-QD are a ripple divide by 8 on CLK2 falling
// (QA to CLK2 makes 16). CLR_N low clears every Q, asynchronously and first; CT_LOAD_N low makes
// each Q follow its data input A-D, asynchronously (a transparent latch). Counting needs both high.
class Ttl177 final : public Component {
public:
    explicit Ttl177(std::string reference);
    static constexpr PinIndex kLoadN = 0, kQC = 1, kC = 2, kA = 3, kQA = 4, kClk2 = 5, kClk1 = 7,
                              kQB = 8, kB = 9, kD = 10, kQD = 11, kClrN = 12;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    std::uint8_t count_ = 0;    // QA..QD, bit 0 first
    std::uint8_t unknown_ = 0;  // bits at X
};

}  // namespace bw
