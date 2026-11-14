#pragma once

#include <cstddef>
#include <optional>

#include "sim/ids.hpp"

namespace bw {

// A memory Part whose cells a Fault can stick (ARCHITECTURE, Faults: internal faults): the 2114
// and the ROMs. Words are the chip's own addresses (its A pins, bit 0 first); bits are its data
// Pins, bit 0 first.
class MemoryCells {
public:
    virtual ~MemoryCells() = default;
    virtual std::size_t wordCount() const = 0;
    // Bit of a word stored for data Pin `pin`; nullopt if `pin` is not a data Pin.
    virtual std::optional<unsigned> dataBit(PinIndex pin) const = 0;
    // From now on bit `bit` of word `word` reads `one`, whatever is written or powered on.
    virtual void stick(std::size_t word, unsigned bit, bool one) = 0;
};

}  // namespace bw
