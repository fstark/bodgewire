#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "parts/memory_block.hpp"
#include "parts/memory_cells.hpp"
#include "sim/component.hpp"

namespace bw {

// A KIM-1 mask (docs/p5j-bw1-chips/BW1_CHIPS.md): ROM while RS0 is low; RAM and I/O while RS0 is
// high, CS1 low and A9-A6 equal `ram` or `io`.
struct Mos6530Mask {
    std::string_view name;  // "002"
    unsigned ram;           // A9-A6: 002 0b1111 ($17C0), 003 0b1110 ($1780)
    unsigned io;            // A9-A6: 002 0b1101 ($1740), 003 0b1100 ($1700)
};
const Mos6530Mask* FindMos6530Mask(std::string_view name);  // nullptr if unknown

// MOS 6530 ROM-RAM-I/O-timer (P5j, docs/p5j-bw1-chips/BW1_CHIPS_DESIGN.md §6), NMOS: 1024 x 8 ROM,
// 64 x 8 RAM, ports A and B with direction registers, an interval timer interrupting on PB7.
// Pins per the MCS6530 preliminary data sheet (Aug. 1975, p. 12): 1 GND, 2 PA0, 3 PHI2, 4 RS0,
// 5-8 A9-A6, 9 RW, 10-15 A5-A0, 16 RES_N, 17 PB7, 18 CS1, 19 PB5, 20 VCC, 21-25 PB4-PB0,
// 26-33 D7-D0, 34-40 PA7-PA1. Only PHI2 wakes it; every other input is read at its edges.
class Mos6530 final : public Component, public MemoryCells {
public:
    Mos6530(std::string reference, const Mos6530Mask& mask, std::vector<std::uint8_t> rom);
    static std::vector<PinSpec> Pins();

    static constexpr PinIndex kPhi2 = 2, kRs0 = 3, kRw = 8, kResN = 15, kPb7 = 16, kCs1 = 17;
    static constexpr std::array<PinIndex, 10> kAddress{14, 13, 12, 11, 10, 9, 7, 6, 5, 4};  // A0..
    static constexpr std::array<PinIndex, 8> kData{32, 31, 30, 29, 28, 27, 26, 25};         // D0..
    static constexpr std::array<PinIndex, 8> kPortA{1, 39, 38, 37, 36, 35, 34, 33};         // PA0..
    // PB0-PB7 by bit; PB6 is CS1 on the KIM masks: no Pin.
    static constexpr PinIndex kNoPin = 0xFFFF;
    static constexpr std::array<PinIndex, 8> kPortB{24, 23, 22, 21, 20, 18, kNoPin, 16};

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;
    std::uint8_t ram(std::size_t address) const { return ram_[address]; }  // tests

    std::span<const std::string_view> memories() const override;
    std::size_t wordCount(std::size_t memory) const override;
    std::optional<unsigned> dataBit(PinIndex pin) const override;  // D0..D7: 0..7
    void stick(std::size_t memory, std::size_t word, unsigned bit, bool one) override;

private:
    struct Access {
        bool rom, ram, io;
        unsigned address;  // A9..A0
    };
    Access decode(EvalContext& ctx, bool before);
    void rise(EvalContext& ctx);
    void fall(EvalContext& ctx);
    std::uint8_t readRegister(EvalContext& ctx, unsigned address);
    void writeRegister(unsigned address, std::uint8_t value);
    std::uint8_t timerValue() const;
    void drivePorts(EvalContext& ctx, Time delay);  // the Pins whose value changed

    const Mos6530Mask& mask_;
    RomBlock rom_;
    RamBlock ram_{64, 8};
    std::array<std::uint8_t, 2> port_{}, direction_{};  // A, B
    std::array<std::uint8_t, 2> driven_{};              // last value driven on each port's Pins
    std::uint64_t pulse_ = 0;        // φ2 rises since power-on: pulse k of the data sheet's Fig. 5
    std::uint64_t timerStart_ = 0;   // pulse of the last timer write
    std::uint64_t interruptAt_ = 0;  // timerStart_ + N·P: the flag's pulse
    std::uint8_t timerCount_ = 0;    // N
    unsigned timerShift_ = 10;       // P = 1 << shift: 0, 3, 6, 10
    bool flagPending_ = false;       // the flag will set at interruptAt_
    bool flag_ = false, irqEnabled_ = false, driving_ = false;
};

}  // namespace bw
