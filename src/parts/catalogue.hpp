#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include "sim/circuit.hpp"
#include "sim/component.hpp"

namespace bw {

enum class PartKind : std::uint8_t { Chip, Resistor };

struct PartParam {
    std::string name;
    std::string value;
};

// Creates the Component in `circuit`, named `reference`; error text on a bad parameter.
using PartFactory = std::expected<Component*, std::string> (*)(Circuit& circuit,
                                                               std::string reference,
                                                               std::span<const PartParam> params);

struct PartInfo {
    std::string_view name;         // "74LS161"
    std::string_view description;  // "synchronous 4-bit binary counter"
    std::string_view package;      // "DIP-16"
    PartKind kind;
    PartFactory create;  // nullptr for resistors (no Component)
};

const PartInfo* FindPart(std::string_view name);  // nullptr if unknown
std::span<const PartInfo> AllParts();

}  // namespace bw
