#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "sim/component.hpp"

namespace bw {

// MOS 6520 peripheral interface adapter (PIA; the Motorola 6820/6821), NMOS (P5n,
// docs/p5n-bw2001-chips/BW2001_CHIPS_DESIGN.md §5). Two 8-bit ports with direction registers, two
// control lines a side (C1 an edge input, C2 an input or a handshake, pulse or manual output) and
// an IRQ output a side. Pins per the MCS6520 data sheet (p. 1): 1 GND, 2-9 PA0-PA7, 10-17 PB0-PB7,
// 18 CB1, 19 CB2, 20 VCC, 21 RW, 22 CS0, 23 CS2_N, 24 CS1, 25 PHI2, 26-33 D7-D0, 34 RES_N, 35 RS1,
// 36 RS0, 37 IRQB_N, 38 IRQA_N, 39 CA2, 40 CA1. PA and CA2 have pull-ups; PB and CB2 are
// three-state; the IRQs are open drain. Only PHI2 wakes it; every other input is read at its edges.
class Mos6520 final : public Component {
public:
    explicit Mos6520(std::string reference);
    static std::vector<PinSpec> Pins();

    static constexpr PinIndex kCb1 = 17, kCb2 = 18, kRw = 20, kCs0 = 21, kCs2N = 22, kCs1 = 23,
                              kPhi2 = 24, kResN = 33, kRs1 = 34, kRs0 = 35, kIrqBN = 36,
                              kIrqAN = 37, kCa2 = 38, kCa1 = 39;
    static constexpr std::array<PinIndex, 8> kPortA{1, 2, 3, 4, 5, 6, 7, 8};         // PA0..
    static constexpr std::array<PinIndex, 8> kPortB{9, 10, 11, 12, 13, 14, 15, 16};  // PB0..
    static constexpr std::array<PinIndex, 8> kData{32, 31, 30, 29, 28, 27, 26, 25};  // D0..

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    // One side: A (0) or B (1).
    struct Side {
        std::uint8_t output = 0, direction = 0, control = 0;  // OR, DDR, CR bits 5-0
        bool irq1 = false, irq2 = false;                      // CR bits 7, 6
        bool c1 = true, c2 = true;                            // C1, C2 as last sampled
        bool c2Out = true;                                    // C2's level as an output
        bool strobe = false;   // a handshake or pulse strobe to start (A: at this fall)
        bool pulseOn = false;  // a pulse strobe in progress, to end at the next edge
    };

    void sample(EvalContext& ctx);
    void reset();
    void rise(EvalContext& ctx);
    void fall(EvalContext& ctx);
    bool selected(EvalContext& ctx, bool before) const;
    unsigned registerOf(EvalContext& ctx, bool before) const;  // RS1 RS0
    std::uint8_t read(EvalContext& ctx, unsigned reg);
    void write(unsigned reg, std::uint8_t value);
    void driveOutputs(EvalContext& ctx);
    void drive(EvalContext& ctx, PinIndex pin, LogicValue value, Time delay);

    std::array<Side, 2> side_{};
    std::array<LogicValue, 40> driven_{};  // what each driven Pin was last given
    bool sampled_ = false;                 // the C lines have been sampled once
    bool driving_ = false;                 // D0-D7 driven this cycle
};

}  // namespace bw
