#pragma once

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
#include "sim/time.hpp"

namespace bw {

struct RomEnable {
    PinIndex pin;  // active low unless the Part's select= says otherwise (2316B): every enable
                   // active drives the outputs, else they float
    Time delay;    // enable to output, ps
};

// One ROM Part of the catalogue (2716, 2513, 2316B): its pins and timing.
// A Rom's own state between evaluations.
struct RomState {
    // When the word at the current address is valid (the last address change plus its access
    // time); a select times its data from it too (P5v).
    Time dataReady = 0;
    bool quiet = false;  // off (an enable at its inactive level): the address Pins quiet (P5z)
};

struct RomSpec {
    std::string_view part;                    // "2716"
    std::span<const std::string_view> names;  // index = pin - 1
    std::span<const PinIndex> address;        // bit 0 first
    std::span<const Time> addressDelay;       // per address bit, ps
    std::span<const PinIndex> data;           // bit 0 first, at most 8
    std::span<const RomEnable> enables;
    std::span<const PinIndex> otherInputs;  // inputs with no function in a read ("VPP")
    Time offDelay;                          // enable high to outputs off, ps
    // EvaluateRom<its layout>: evaluation specialised on these pins at compile time (rom.cpp).
    // activeHigh: bit i set when enable i is active high (2316B select=); 0 otherwise.
    void (*evaluate)(EvalContext& ctx, std::span<const std::uint8_t> words, unsigned activeHigh,
                     RomState& state);
    bool selectable = false;  // select= sets each enable's polarity, required (2316B)
};

const RomSpec* FindRomSpec(std::string_view part);  // nullptr unless a ROM Part
const RomSpec& RomSpecOf(std::string_view part);    // Check()s

// Read-only memory: the word at the address on the data Pins while enabled, floating otherwise.
// `words` holds 2^address bits words of the data width (ROM_PARTS_DESIGN.md §3). A stuck cell
// (MemoryCells) changes the contents.
class Rom final : public Component, public MemoryCells {
public:
    Rom(std::string reference, const RomSpec& spec, std::vector<std::uint8_t> words,
        unsigned activeHigh = 0);
    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;
    std::size_t size() const { return block_.size(); }
    std::uint8_t word(std::size_t address) const { return block_[address]; }  // as held

    std::size_t wordCount(std::size_t memory) const override;
    std::optional<unsigned> dataBit(PinIndex pin) const override;  // index in the spec's data
    void stick(std::size_t memory, std::size_t word, unsigned bit, bool one) override;

private:
    const RomSpec& spec_;
    RomBlock block_;
    unsigned activeHigh_;
    RomState state_;
};

}  // namespace bw
