#pragma once

#include <span>
#include <string>
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

struct BoardPower {
    std::vector<std::string> unpowered;  // chip and PADDLE References, design order
    bool deadRail = false;               // an island holds supply pins of different voltages
};

// Powered check of every chip and PADDLE controller on `board`'s islands. The POWER and VIDEO
// connectors are always Powered.
BoardPower FindPower(const LoadedCircuit& designCircuit, const Board& board,
                     const Islands& islands);

}  // namespace bw
