#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "netlist/parser.hpp"

namespace bw {

// ROMs chosen at run time (P5q2): replaces the contents= of each ROM Part (a Part with
// contents=) that has a file named after its reference in `directory` (UH1.bin or UH1.rom), and
// drops its offset=; other files are ignored. Call after ResolveContentsPaths, before
// BuildCircuit. The directory is made absolute: the files are read again at every build. Errors:
// the directory missing; a REF.bin or REF.rom matching no ROM Part; two files for one Part.
std::expected<void, std::string> ApplyRomDirectory(Netlist& netlist,
                                                   const std::filesystem::path& directory);

}  // namespace bw
