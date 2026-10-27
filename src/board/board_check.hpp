#pragma once

#include <string>
#include <vector>

#include "board/board.hpp"
#include "netlist/builder.hpp"

namespace bw {

// Problems with `board` against the netlist, one line each; empty = ok.
std::vector<std::string> CheckBoard(const LoadedCircuit& loaded, const Board& board);

}  // namespace bw
