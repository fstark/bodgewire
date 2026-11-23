#pragma once

#include <cstdint>
#include <expected>
#include <optional>
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

// "U3.7" or "U3.1Y": a reference, one dot, a pin; nullopt if either is empty or a second dot
// follows. The reference is not checked against IsReference.
std::optional<PinRef> ParsePinRef(std::string_view text);

// [A-Z]+[0-9]+: "U12", "SW1".
bool IsReference(std::string_view text);

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
