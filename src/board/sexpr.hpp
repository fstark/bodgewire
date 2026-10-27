#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "netlist/diagnostic.hpp"

namespace bw {

// One node of an S-expression file (KiCad): an atom (quoted or not) or a list.
struct SExpr {
    std::uint32_t line = 0;
    bool isList = false;
    std::string atom;
    std::vector<SExpr> list;

    std::string_view head() const;                     // first atom of a list, "" otherwise
    const SExpr* find(std::string_view head) const;    // first child list with this head
    std::string_view atomAt(std::size_t index) const;  // list[index] atom, "" if absent
};

std::expected<SExpr, Diagnostic> ParseSExpr(std::string_view text, std::string_view file);

}  // namespace bw
