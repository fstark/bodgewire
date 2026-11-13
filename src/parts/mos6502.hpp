#pragma once

#include <array>
#include <string>

#include "parts/mos6502_core.hpp"
#include "sim/component.hpp"

namespace bw {

// NMOS 6502 (P5c, docs/p5c-6502/MOS6502_DESIGN.md): Mos6502Core behind the Pins, cycle-exact per
// PHI0 edge. Pins per the MOS 6500 data sheet (Nov. 1985, p. 10): 1 GND, 2 RDY, 3 PHI1, 4 IRQ_N,
// 5 NC1, 6 NMI_N, 7 SYNC, 8 VCC, 9-20 A0-A11, 21 GND2, 22-25 A12-A15, 26-33 D7-D0, 34 RW,
// 35 NC2, 36 NC3, 37 PHI0, 38 SO, 39 PHI2, 40 RES_N. Only PHI0 wakes it; every other input is
// sampled at its edges.
class Mos6502 final : public Component {
public:
    explicit Mos6502(std::string reference);

    static constexpr PinIndex kRdy = 1, kPhi1 = 2, kIrqN = 3, kNmiN = 5, kSync = 6, kRw = 33,
                              kPhi0 = 36, kSo = 37, kPhi2 = 38, kResN = 39;
    static constexpr std::array<PinIndex, 16> kAddress{8,  9,  10, 11, 12, 13, 14, 15,
                                                       16, 17, 18, 19, 21, 22, 23, 24};  // A0..
    static constexpr std::array<PinIndex, 8> kData{32, 31, 30, 29, 28, 27, 26, 25};      // D0..

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

    const Mos6502Core& core() const { return core_; }

private:
    void phase2(EvalContext& ctx);
    void endCycle(EvalContext& ctx);

    Mos6502Core core_;
    bool so_ = true;  // SO at the last PHI0 rise
};

}  // namespace bw
