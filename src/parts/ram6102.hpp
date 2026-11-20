#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "parts/memory_block.hpp"
#include "parts/memory_cells.hpp"
#include "sim/component.hpp"

namespace bw {

// 1K x 1 static RAM (MOS 6102, the Intel 2102's second source), MOS. Pins as the KIM-1 wires it
// (User Manual Fig. 3.4): 1 A6, 2 A5, 3 RW, 4 A1, 5 A2, 6 A3, 7 A4, 8 A0, 9 GND, 10 VCC, 11 DIN,
// 12 DOUT, 13 CE_N, 14 A9, 15 A8, 16 A7. CE_N low, RW high: DOUT drives the bit at A0-A9. CE_N and
// RW low: DIN is written; the address and data held just before the write ends are kept; DOUT
// floats (assumed, as the 2114). CE_N high: DOUT floats. Inputs read X as a random bit.
class Ram6102 final : public Component, public MemoryCells {
public:
    explicit Ram6102(std::string reference);
    static constexpr PinIndex kRw = 2, kDin = 10, kDout = 11, kCeN = 12;
    static constexpr std::array<PinIndex, 10> kAddress{7, 3, 4, 5, 6, 1, 0, 15, 14, 13};  // A0..A9

    void powerOn(PowerOnContext& ctx) override;  // random contents, DOUT released
    void evaluate(EvalContext& ctx) override;
    bool bit(std::size_t address) const { return cells_[address] != 0; }  // tests

    std::size_t wordCount(std::size_t memory) const override;
    std::optional<unsigned> dataBit(PinIndex pin) const override;  // DIN and DOUT: 0
    void stick(std::size_t memory, std::size_t word, unsigned bit, bool one) override;
    std::uint8_t read(std::size_t memory, std::size_t word) const override;
    unsigned wordBits(std::size_t memory) const override;

private:
    RamBlock cells_{1024, 1};
};

}  // namespace bw
