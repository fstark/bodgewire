#pragma once

#include "board/board.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"

namespace bw {

// The Nets the Board's Copper actually makes (S1): one per island, named after the design Net
// of its Pins (NAME, NAME~2, ...). `designCircuit` is `design` built, `board` matches it.
Netlist ElectricalNetlist(const Netlist& design, const LoadedCircuit& designCircuit,
                          const Board& board);

}  // namespace bw
