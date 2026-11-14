#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "sim/ids.hpp"

namespace bw {

// A memory Part whose cells a Fault can stick (ARCHITECTURE, Faults: internal faults): the 2114,
// the ROMs, the 6102 and the 6530. Words are a memory's own addresses (bit 0 first); bits are the
// chip's data Pins, bit 0 first.
class MemoryCells {
public:
    virtual ~MemoryCells() = default;
    // Its memories' names: {""} for a chip with one (2114, ROMs, 6102); {"rom", "ram"} (6530).
    virtual std::span<const std::string_view> memories() const {
        static constexpr std::array<std::string_view, 1> kOne{""};
        return kOne;
    }
    virtual std::size_t wordCount(std::size_t memory) const = 0;
    // Bit of a word stored for data Pin `pin`; nullopt if `pin` is not a data Pin.
    virtual std::optional<unsigned> dataBit(PinIndex pin) const = 0;
    // From now on bit `bit` of word `word` of memory `memory` reads `one`, whatever is written or
    // powered on.
    virtual void stick(std::size_t memory, std::size_t word, unsigned bit, bool one) = 0;
};

}  // namespace bw
