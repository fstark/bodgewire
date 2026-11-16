#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "board/board.hpp"
#include "netlist/diagnostic.hpp"

namespace bw {

struct FootprintPad {
    std::string number;  // "1"; Pin index + 1
    Point position;      // relative to the footprint origin, at 0°
};

// A KiCad library footprint (.kicad_mod), as vendored in footprints.
struct Footprint {
    std::string id;    // "Package_DIP:DIP-14_W7.62mm", as in the netlist
    std::string text;  // the .kicad_mod file, unchanged
    std::vector<FootprintPad> pads;
    Point courtyardMin, courtyardMax;  // bounding box of the F.CrtYd graphics and the pads
};

// By package ("DIP-14").
using FootprintLibrary = std::map<std::string, Footprint, std::less<>>;

Footprint ReadFootprint(std::string_view text, std::string_view id, std::string_view file,
                        std::vector<Diagnostic>& errors);

// Only the packages asked for are read; diagnostics name the file.
std::expected<FootprintLibrary, std::vector<Diagnostic>>
LoadFootprints(const std::filesystem::path& directory, std::span<const std::string_view> packages);

}  // namespace bw
