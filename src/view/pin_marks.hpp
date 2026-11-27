#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sim/logic.hpp"

namespace bw {

// A pin's symbol on a chip's pinout and beside its name plate on the Board (P8j): one colour for
// every kind, the kind told by the shape.
enum class PinMark : std::uint8_t { In, Out, OpenCollector, Both, Power, NoConnect, Passive };

struct PinLook {
    PinMark mark = PinMark::Passive;
    bool activeLow = false;  // the name ends in _N: a bubble at the body, the name unchanged
    bool clock = false;      // CLK, CLK1, 1CLK, CKA, PHI, PHI2…: a pulse over the line
    bool operator==(const PinLook&) const = default;
};

// A chip's pin (`chip`: its PartKind is Chip) by its kind: Input in, Output out, OpenCollector its
// own, Bidirectional and PullUp (the ports, a bus: ADD_PART.md) both ways, Power, NoConnect; the
// rest Passive. Anything else's pins (a resistor, LED, digit, switch, connector: their kinds say
// how they are simulated, not what they are) are Passive, but power by kind or by a name that
// starts with VCC or GND. `kind` nullopt (a Part without a Component): Passive.
PinLook LookOf(std::string_view name, std::optional<PinKind> kind, bool chip);

// The pins, as indices into `names` (a Part's pin names in pad order, `kinds` beside them), that
// pin `pin` works with, itself included, ascending. A section's pin has a name of digits then a
// letter ("3A", "1CLK", "1A1"); a Part with one has sections. A section's pin works with its
// section's pins and the shared ones (no leading digit), a shared pin with every section's pins
// (and itself). Power and NC pins (by kind), pins without a kind and Parts without sections:
// nothing (empty).
std::vector<std::size_t> WorksWith(std::span<const std::string> names,
                                   std::span<const std::optional<PinKind>> kinds, std::size_t pin);

}  // namespace bw
