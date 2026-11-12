#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "sim/component.hpp"

namespace bw {

// Benchmark stand-in for a Z80 (P5a, docs/p5a-bus-load/BUS_LOAD.md): executes nothing, plays a
// fixed ZX80-like frame of machine cycles with Z80 pin timing, 312 lines of 207 T-states. Pins per
// the Z80 datasheet: 1-5 A11-A15, 6 CLK, 7 D4, 8 D3, 9 D5, 10 D6, 11 VCC, 12 D2, 13 D7, 14 D0,
// 15 D1, 16 INT_N, 17 NMI_N, 18 HALT_N, 19 MREQ_N, 20 IORQ_N, 21 RD_N, 22 WR_N, 23 BUSAK_N,
// 24 WAIT_N, 25 BUSRQ_N, 26 RESET_N, 27 M1_N, 28 RFSH_N, 29 GND, 30-40 A0-A10. Inputs other than
// CLK are not read.
class Z80Stub final : public Component {
public:
    explicit Z80Stub(std::string reference);

    static constexpr PinIndex kClk = 5, kHaltN = 17, kMreqN = 18, kIorqN = 19, kRdN = 20, kWrN = 21,
                              kBusakN = 22, kM1N = 26, kRfshN = 27;
    static constexpr std::array<PinIndex, 16> kAddress{29, 30, 31, 32, 33, 34, 35, 36,
                                                       37, 38, 39, 0,  1,  2,  3,  4};  // A0..A15
    static constexpr std::array<PinIndex, 8> kData{13, 14, 11, 7, 6, 8, 9, 12};         // D0..D7

    static constexpr int kLineTStates = 207;
    static constexpr int kFrameLines = 312;
    static constexpr int kRows = 24, kColumns = 32, kRowBytes = kColumns + 1;
    static constexpr int kFirstDisplayLine = 52;
    static constexpr std::uint16_t kRefreshPage = 0x0E00;  // I = 0x0E

    // The display file the writes store: 24 rows of 32 character codes and a NEWLINE (0x76).
    static const std::array<std::uint8_t, kRows * kRowBytes>& Page();

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

    int line() const;    // of the cycle playing, 0..311
    int tState() const;  // within the line, 0..206

    enum class Kind : std::uint8_t {
        Fetch,
        HaltFetch,
        Read,
        Write,
        In,
        Out,
        InterruptAcknowledge,
        Internal
    };
    struct Cycle {
        Kind kind;
        std::uint16_t address;
        std::uint8_t data;
        std::uint16_t line;
        std::uint16_t start;  // T-state within the line
    };
    const std::vector<Cycle>& frame() const { return frame_; }

private:
    void rise(EvalContext& ctx);
    void fall(EvalContext& ctx);
    void driveAddress(EvalContext& ctx, std::uint16_t address);
    void driveData(EvalContext& ctx, std::uint8_t data);
    void releaseData(EvalContext& ctx);

    std::vector<Cycle> frame_;
    std::size_t cycle_ = 0;
    int t_ = -1;  // T-state within the cycle, 0-based; -1 before the first CLK rise
    std::uint8_t r_ = 0;
    bool dataDriven_ = false;
};

}  // namespace bw
