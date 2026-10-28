#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "board/board.hpp"
#include "netlist/diagnostic.hpp"
#include "sim/logic.hpp"

namespace bw {

// Syntax tree of a .bwp file; no Machine knowledge.
struct StuckAtDecl {
    std::uint32_t line;
    std::string reference, pin;
    LogicValue value;  // Zero or One
};

struct CrackDecl {
    std::uint32_t line;
    Layer layer;
    Point at;  // nm, KiCad coordinates
};

struct Puzzle {
    std::string file;
    std::uint32_t machineLine = 0;
    std::string machine;       // "test_pattern": [a-z0-9_]+, so it is safe in a path
    std::string title, brief;  // "" when absent
    std::uint32_t titleLine = 0, briefLine = 0;
    std::vector<StuckAtDecl> stuck;
    std::vector<CrackDecl> cracks;
};

// All syntax errors, in line order.
std::expected<Puzzle, std::vector<Diagnostic>> ParsePuzzle(std::string_view text, std::string file);

std::string_view LayerName(Layer layer);  // "F.Cu", "B.Cu"

}  // namespace bw
