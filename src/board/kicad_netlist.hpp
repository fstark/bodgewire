#pragma once

#include <ostream>
#include <string_view>

#include "netlist/builder.hpp"

namespace bw {

std::string_view KicadFootprint(std::string_view package);  // "" if unknown

// KiCad S-expression netlist for pcbnew's "Import Netlist"; `source` names the .bwn file.
void WriteKicadNetlist(std::ostream& out, const LoadedCircuit& loaded, std::string_view source);

}  // namespace bw
