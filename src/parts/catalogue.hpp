#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "sim/circuit.hpp"
#include "sim/component.hpp"

namespace bw {

// Led: Parts made of LEDs (an LED, a 7-segment digit); what they light is in PartInfo::lamps.
enum class PartKind : std::uint8_t { Chip, Resistor, Connector, Led, Button };

enum class LampShape : std::uint8_t {
    Round,
    SegmentA,
    SegmentB,
    SegmentC,
    SegmentD,
    SegmentE,
    SegmentF,
    SegmentG,
    DecimalPoint
};

// One light of a Part: lit while an anode Pin reads a driven 1 and the cathode Pin a driven 0.
struct Lamp {
    LampShape shape;
    std::array<std::string_view, 2> anodes;  // Pin names; the second "" if one
    std::string_view cathode;
};

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
    PartFactory create;             // nullptr for resistors (no Component)
    std::span<const Lamp> lamps{};  // empty: the Part does not light
};

const PartInfo* FindPart(std::string_view name);  // nullptr if unknown
std::span<const PartInfo> AllParts();

// Voltage a chip's power Pin expects: "VCC" 5, "GND" and "GND2" 0; nullopt for other names.
std::optional<double> PowerPinVolts(std::string_view pin);

// The rail Net a chip's power Pin belongs on: its own name, but GND for a second ground (GND2).
std::string_view PowerPinRail(std::string_view pin);

}  // namespace bw
