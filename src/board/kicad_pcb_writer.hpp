#pragma once

#include <ostream>
#include <string_view>

#include "board/placer.hpp"
#include "netlist/builder.hpp"

namespace bw {

// KiCad 10 .kicad_pcb: the footprints placed with their Nets on the pads, the outline; no
// tracks. KiCad gives every item a uuid when it saves.
void WriteKicadPcb(std::ostream& out, const LoadedCircuit& loaded, const Placement& placement);

// KiCad project with the design rules every Board uses (KiCad's defaults); `name` is the file
// stem.
void WriteKicadProject(std::ostream& out, std::string_view name);

}  // namespace bw
