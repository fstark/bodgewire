#pragma once

#include <array>
#include <string>
#include <vector>

#include "parts/mos6522_core.hpp"
#include "sim/component.hpp"

namespace bw {

// MOS 6522 versatile interface adapter (VIA), NMOS (P5n, docs/p5n-bw2001-chips/
// BW2001_CHIPS_DESIGN.md §6): its logic in Mos6522Core, one φ2 cycle at a time. Pins per the R6522
// data sheet (p. 1): 1 GND, 2-9 PA0-PA7, 10-17 PB0-PB7, 18 CB1, 19 CB2, 20 VCC, 21 IRQ_N, 22 RW,
// 23 CS2_N, 24 CS1, 25 PHI2, 26-33 D7-D0, 34 RES_N, 35 RS3, 36 RS2, 37 RS1, 38 RS0, 39 CA2, 40 CA1.
// The ports, CA2, CB1 and CB2 are pull-up Pins; IRQ_N is open drain. Only PHI2 wakes it; the
// inputs are read at its rise, a write at its fall.
class Mos6522 final : public Component {
public:
    explicit Mos6522(std::string reference);
    static std::vector<PinSpec> Pins();

    static constexpr PinIndex kCb1 = 17, kCb2 = 18, kIrqN = 20, kRw = 21, kCs2N = 22, kCs1 = 23,
                              kPhi2 = 24, kResN = 33, kCa2 = 38, kCa1 = 39;
    static constexpr std::array<PinIndex, 8> kPortA{1, 2, 3, 4, 5, 6, 7, 8};         // PA0..
    static constexpr std::array<PinIndex, 8> kPortB{9, 10, 11, 12, 13, 14, 15, 16};  // PB0..
    static constexpr std::array<PinIndex, 8> kData{32, 31, 30, 29, 28, 27, 26, 25};  // D0..
    static constexpr std::array<PinIndex, 4> kRs{37, 36, 35, 34};                    // RS0..

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;
    const Mos6522Core& core() const { return core_; }  // tests

private:
    void rise(EvalContext& ctx);
    void fall(EvalContext& ctx);
    void driveOutputs(EvalContext& ctx);
    void drive(EvalContext& ctx, PinIndex pin, LogicValue value, Time delay);

    Mos6522Core core_;
    std::array<LogicValue, 40> driven_{};
    bool driving_ = false;
};

}  // namespace bw
