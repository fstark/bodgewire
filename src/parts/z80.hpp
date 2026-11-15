#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "parts/z80_core.hpp"
#include "sim/component.hpp"

namespace bw {

// NMOS Z80 (P5s, docs/p5s-z80/Z80_DESIGN.md): Z80Core behind the Pins, cycle-exact per CLK edge.
// Pins per the Zilog Z80 CPU User Manual (UM0080): 1-5 A11-A15, 6 CLK, 7 D4, 8 D3, 9 D5, 10 D6,
// 11 VCC, 12 D2, 13 D7, 14 D0, 15 D1, 16 INT_N, 17 NMI_N, 18 HALT_N, 19 MREQ_N, 20 IORQ_N,
// 21 RD_N, 22 WR_N, 23 BUSAK_N, 24 WAIT_N, 25 BUSRQ_N, 26 RESET_N, 27 M1_N, 28 RFSH_N, 29 GND,
// 30-40 A0-A10. Only CLK wakes it; every other input is sampled at its edges.
class Z80 final : public Component {
public:
    explicit Z80(std::string reference);

    static constexpr PinIndex kClk = 5, kIntN = 15, kNmiN = 16, kHaltN = 17, kMreqN = 18,
                              kIorqN = 19, kRdN = 20, kWrN = 21, kBusakN = 22, kWaitN = 23,
                              kBusrqN = 24, kResetN = 25, kM1N = 26, kRfshN = 27;
    static constexpr std::array<PinIndex, 16> kAddress{29, 30, 31, 32, 33, 34, 35, 36,
                                                       37, 38, 39, 0,  1,  2,  3,  4};  // A0..A15
    static constexpr std::array<PinIndex, 8> kData{13, 14, 11, 7, 6, 8, 9, 12};         // D0..D7

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

    const Z80Core& core() const { return core_; }

private:
    // The T-state within the cycle; Wa1 and Wa2 are the automatic waits of I/O and the
    // acknowledge, Tw an asked-for one, Internal one of an Internal cycle's.
    enum class Phase : std::uint8_t { Next, T1, T2, Wa1, Wa2, Tw, T3, T4, Internal };
    enum class Mode : std::uint8_t { Running, Granted, Reset };

    void rise(EvalContext& ctx);
    void fall(EvalContext& ctx);
    Phase following() const;  // the phase after this one, Next when the cycle ends
    bool lastPhase() const;   // this T-state is the cycle's last
    void startCycle(EvalContext& ctx);
    void lastRise(EvalContext& ctx);
    void driveAddress(EvalContext& ctx, std::uint16_t address);
    void driveData(EvalContext& ctx, std::uint8_t data);
    void releaseData(EvalContext& ctx, Time delay);
    void floatBus(EvalContext& ctx);
    std::uint8_t readData(EvalContext& ctx);

    Z80Core core_;
    Z80Bus cycle_;
    Phase phase_ = Phase::Next;
    Mode mode_ = Mode::Running;
    int internalLeft_ = 0;
    std::uint8_t latched_ = 0;
    bool waitAsked_ = false, busRequested_ = false;
    bool dataDriven_ = false, floating_ = false, refreshing_ = false, haltLow_ = false;
};

}  // namespace bw
