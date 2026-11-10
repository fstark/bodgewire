#pragma once

#include <cstdint>
#include <expected>
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

struct PlaceRule {
    std::uint32_t line;
    std::variant<EdgeRule, NextRule> rule;
};

// A .place file: how the panel parts of a Board are placed.
struct PlaceRules {
    std::vector<PlaceRule> rules;                               // file order
    std::optional<std::pair<std::int32_t, std::int32_t>> grid;  // columns, rows
};

std::expected<PlaceRules, std::vector<Diagnostic>> ParsePlaceRules(std::string_view text,
                                                                   std::string file);

std::string_view EdgeName(Edge edge);  // "left"

}  // namespace bw
