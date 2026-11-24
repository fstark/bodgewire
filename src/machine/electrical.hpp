#pragma once

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "board/board.hpp"
#include "board/copper.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"

namespace bw {

// Electrical Net name of each island of `board`, as ElectricalNetlist names it; "" for an island
// no Pin names (bare Copper, a left-out resistor's pad).
std::vector<std::string> IslandNetNames(const LoadedCircuit& designCircuit, const Board& board,
                                        const Islands& islands);

// The Nets the Board's Copper actually makes (S1): one per island, named after the design Net
// of its Pins (NAME, NAME~2, ...). `designCircuit` is `design` built, `board` matches it.
// `dry`: chip pins (by number) whose Joint does not connect: on no Net, they float.
Netlist ElectricalNetlist(const Netlist& design, const LoadedCircuit& designCircuit,
                          const Board& board, std::span<const PinRef> dry = {});
// The same with `board`'s islands already worked out (FindIslands is most of the cost).
Netlist ElectricalNetlist(const Netlist& design, const LoadedCircuit& designCircuit,
                          const Board& board, const Islands& islands,
                          std::span<const PinRef> dry = {});

struct BoardPower {
    std::vector<std::string> unpowered;  // chip and PADDLE References, design order
    bool deadRail = false;               // an island holds supply pins of different voltages
};

// Powered check of every chip and PADDLE controller on `board`'s islands. The POWER and VIDEO
// connectors are always Powered.
BoardPower FindPower(const LoadedCircuit& designCircuit, const Board& board,
                     const Islands& islands);

// What a Circuit is built from on the Copper's side (P8c): each electrical Net's name and pins,
// and the chips the Copper leaves Unpowered. Two Boards of one design with equal Wiring build the
// same Circuit, whatever Copper made it (a mend or a Bodge Wire). Names count: the builder ties
// the Nets named VCC and GND to the supplies.
struct Wiring {
    // Each Net's name and its pins ("U3.7", "R2.1") sorted; the Nets sorted by name.
    std::vector<std::pair<std::string, std::vector<std::string>>> nets;
    std::vector<std::string> unpowered;  // BoardPower's, design order
    bool deadRail = false;
    bool operator==(const Wiring&) const = default;
};

// ElectricalNetlist (no dry Joints: they are Faults of their own) of `board`, whose islands are
// `islands`, and its `power` (FindPower of the same).
Wiring WiringOf(const Netlist& design, const LoadedCircuit& designCircuit, const Board& board,
                const Islands& islands, const BoardPower& power);

}  // namespace bw
