#pragma once

#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "netlist/diagnostic.hpp"

namespace bw {

// A Board edge; the front is the player's side, at the bottom of the Board view. Not "Side": in
// the glossary a Side is a Layer.
enum class Edge : std::uint8_t { Left, Right, Back, Front };

// Parts against an edge (`edge`, `row`), in order from its left or back end. `at` along the edge:
// 0 = the left (or back) end, 1 = the right (or front) end.
struct EdgeRule {
    Edge edge;
    double at;
    std::vector<std::string> references;
};

// `reference` just beside `other`, on `side` of it.
struct NextRule {
    std::string reference;
    std::string other;
    Edge side;
};

// `upright REF up|down|in [0|180]` (grid pet, P5q1): the chip stands in its column, reaching up or
// down from its row, or within it; at 180 its notch is at the bottom, at 0 at the top.
enum class UprightReach : std::uint8_t { Up, Down, In };
struct Upright {
    UprightReach reach;
    int angle;  // 0 or 180
    std::uint32_t line;
};

struct PlaceRule {
    std::uint32_t line;
    std::variant<EdgeRule, NextRule> rule;
};

// A .place file: how the panel parts of a Board are placed.
struct PlaceRules {
    std::vector<PlaceRule> rules;                               // file order
    std::optional<std::pair<std::int32_t, std::int32_t>> grid;  // columns, rows
    // `grid pet` (P5q): chips named U<row A-J><column 1-9> at their PET 2001 location, lying
    // horizontally; `next` may then name such a chip.
    bool petGrid = false;
    std::map<std::string, Upright, std::less<>> upright;  // grid pet: chips standing upright
};

std::expected<PlaceRules, std::vector<Diagnostic>> ParsePlaceRules(std::string_view text,
                                                                   std::string file);

std::string_view EdgeName(Edge edge);  // "left"

}  // namespace bw
