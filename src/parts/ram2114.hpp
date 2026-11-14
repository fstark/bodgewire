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

// 1K x 4 static RAM (Intel 2114), MOS, shared data pins. Pins per datasheet: 1 A6, 2 A5, 3 A4,
// 4 A3, 5 A0, 6 A1, 7 A2, 8 CS_N, 9 GND, 10 WE_N, 11 IO4, 12 IO3, 13 IO2, 14 IO1, 15 A9, 16 A8,
// 17 A7, 18 VCC. CS_N low, WE_N high: IO1-IO4 drive the word at A0-A9. CS_N and WE_N low: the
// word is written from IO1-IO4 (floating); the address and data held just before the write ends
// are kept. CS_N high: IO1-IO4 float. Inputs read X as a random bit. Stuck cells (MemoryCells)
// hold their value through writes and power-on.
class Ram2114 final : public Component, public MemoryCells {
public:
    explicit Ram2114(std::string reference);
    static constexpr PinIndex kCsN = 7, kWeN = 9;
    static constexpr std::array<PinIndex, 10> kAddress{4, 5, 6, 3, 2, 1, 0, 16, 15, 14};  // A0..A9
    static constexpr std::array<PinIndex, 4> kData{13, 12, 11, 10};  // IO1..IO4

    void powerOn(PowerOnContext& ctx) override;  // random contents
    void evaluate(EvalContext& ctx) override;
    std::uint8_t word(std::size_t address) const { return cells_[address]; }  // tests, 4 bits

    std::size_t wordCount(std::size_t memory) const override;
    std::optional<unsigned> dataBit(PinIndex pin) const override;  // index in kData
    void stick(std::size_t memory, std::size_t word, unsigned bit, bool one) override;

private:
    RamBlock cells_{1024, 4};
};

}  // namespace bw
