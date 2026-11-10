#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "board/board.hpp"
#include "board/footprint.hpp"
#include "board/place_rules.hpp"
#include "netlist/builder.hpp"
#include "netlist/diagnostic.hpp"

namespace bw {

struct PlacedPart {
    std::string reference;
    const Footprint* footprint;
    Point origin;  // the footprint's (at), 0°
};

struct Placement {
    Point outlineMin, outlineMax;
    std::vector<PlacedPart> parts;  // netlist order
    std::int32_t columns = 0, rows = 0;
    std::vector<std::string> grid;  // row-major slot → reference, "" when empty
    std::int64_t wireLength = 0;    // nm, half-perimeter, VCC and GND left out
};

struct PlaceOptions {
    std::uint64_t seed = 1;
    std::int32_t tries = 4;  // seeds seed … seed + tries − 1, the shortest kept (first on a tie)
};

// Panel parts (those with a rule) by their rules, in bands along the edges; every other part
// (DIPs, resistors) into a grid of slots, by annealing on wire length. Diagnostics name
// `rulesFile`. `footprints` must hold every package of `loaded`.
std::expected<Placement, std::vector<Diagnostic>>
PlaceBoard(const LoadedCircuit& loaded, const PlaceRules& rules, std::string_view rulesFile,
           const FootprintLibrary& footprints, const PlaceOptions& options);

}  // namespace bw
