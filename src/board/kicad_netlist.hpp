#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include "netlist/builder.hpp"

namespace bw {

std::string_view KicadFootprint(std::string_view package);  // "" if unknown

std::string KicadQuote(std::string_view text);  // "text", with " and \\ escaped

// Stable per reference, so re-importing keeps KiCad's links (netlist tstamps, Board paths).
std::string KicadUuid(std::string_view reference);

// KiCad S-expression netlist for pcbnew's "Import Netlist"; `source` names the .bwn file.
void WriteKicadNetlist(std::ostream& out, const LoadedCircuit& loaded, std::string_view source);

}  // namespace bw
