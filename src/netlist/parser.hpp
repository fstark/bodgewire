#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "netlist/diagnostic.hpp"
#include "parts/catalogue.hpp"

namespace bw {

// Syntax tree of a .bwn file; no Part knowledge.
struct PartDecl {
    std::uint32_t line;
    std::string reference;
    std::string part;
    std::vector<PartParam> params;
};

struct PinRef {
    std::string reference;
    std::string pin;  // package pin number or datasheet name
};

struct NetDecl {
    std::uint32_t line;
    std::string name;
    std::vector<PinRef> pins;
};

struct Netlist {
    std::string file;
    std::vector<PartDecl> parts;
    std::vector<NetDecl> nets;
};

// All syntax errors, in line order.
std::expected<Netlist, std::vector<Diagnostic>> ParseNetlist(std::string_view text,
                                                             std::string file);

}  // namespace bw
