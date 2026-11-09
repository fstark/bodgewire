#include "parts/catalogue.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include "parts/button.hpp"
#include "parts/connector.hpp"
#include "parts/digit.hpp"
#include "parts/gate_package.hpp"
#include "parts/led.hpp"
#include "parts/ls107.hpp"
#include "parts/ls161.hpp"
#include "parts/ls164.hpp"
#include "parts/ls175.hpp"
#include "parts/ls47.hpp"
#include "parts/ls74.hpp"
#include "parts/ls90.hpp"
#include "parts/ls93.hpp"
#include "parts/oscillator.hpp"
#include "parts/paddle.hpp"

namespace bw {

namespace {

template <std::size_t N>
struct PartName {
    constexpr PartName(const char (&text)[N]) { std::copy_n(text, N, chars); }
    constexpr std::string_view view() const { return {chars, N - 1}; }
    char chars[N];
};

template <PartName Name>
std::expected<Component*, std::string> CreateGatePackage(Circuit& circuit, std::string reference,
                                                         std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected(std::string(Name.view()) + " takes no parameters");
    }
    return &circuit.add<GatePackage>(std::move(reference), GatePackageSpecOf(Name.view()));
}

template <class T, PartName Name>
std::expected<Component*, std::string> CreateChip(Circuit& circuit, std::string reference,
                                                  std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected(std::string(Name.view()) + " takes no parameters");
    }
    return &circuit.add<T>(std::move(reference));
}

template <PartName Name>
std::expected<Component*, std::string> CreateConnector(Circuit& circuit, std::string reference,
                                                       std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected(std::string(Name.view()) + " takes no parameters");
    }
    return &circuit.add<Connector>(std::move(reference), ConnectorPins(Name.view()));
}

std::expected<Component*, std::string> CreateOscillator(Circuit& circuit, std::string reference,
                                                        std::span<const PartParam> params) {
    const PartParam* freq = nullptr;
    for (const PartParam& param : params) {
        if (param.name != "freq") {
            return std::unexpected("OSC: unknown parameter '" + param.name + "'");
        }
        freq = &param;
    }
    if (freq == nullptr) {
        return std::unexpected("OSC needs freq=<frequency>");
    }
    const auto period = PeriodFromFrequency(freq->value);
    if (!period) {
        return std::unexpected(period.error());
    }
    return &circuit.add<Oscillator>(std::move(reference), *period);
}

std::expected<Component*, std::string> CreatePaddle(Circuit& circuit, std::string reference,
                                                    std::span<const PartParam> params) {
    const PartParam* shortest = nullptr;
    const PartParam* longest = nullptr;
    for (const PartParam& param : params) {
        if (param.name == "min") {
            shortest = &param;
        } else if (param.name == "max") {
            longest = &param;
        } else {
            return std::unexpected("PADDLE: unknown parameter '" + param.name + "'");
        }
    }
    if (shortest == nullptr || longest == nullptr) {
        return std::unexpected("PADDLE needs min=<duration> and max=<duration>");
    }
    const auto low = DurationFromText(shortest->value);
    if (!low) {
        return std::unexpected(low.error());
    }
    const auto high = DurationFromText(longest->value);
    if (!high) {
        return std::unexpected(high.error());
    }
    if (*low < 1'000'000) {
        return std::unexpected("PADDLE: min must be at least 1us");
    }
    if (*high <= *low) {
        return std::unexpected("PADDLE: max must be longer than min");
    }
    return &circuit.add<Paddle>(std::move(reference), *low, *high);
}

std::expected<Component*, std::string> CreateLed(Circuit& circuit, std::string reference,
                                                 std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected("LED takes no parameters");
    }
    return &circuit.add<Led>(std::move(reference));
}

std::expected<Component*, std::string> CreateButton(Circuit& circuit, std::string reference,
                                                    std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected("BUTTON takes no parameters");
    }
    return &circuit.add<Button>(std::move(reference));
}

std::expected<Component*, std::string> CreateDigit(Circuit& circuit, std::string reference,
                                                   std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected("DIGIT takes no parameters");
    }
    return &circuit.add<Digit>(std::move(reference));
}

constexpr std::array<Lamp, 1> kLedLamps{{{LampShape::Round, {"A", ""}, "K"}}};
constexpr std::array<Lamp, 8> kDigitLamps{{
    {LampShape::SegmentA, {"CA1", "CA2"}, "A"},
    {LampShape::SegmentB, {"CA1", "CA2"}, "B"},
    {LampShape::SegmentC, {"CA1", "CA2"}, "C"},
    {LampShape::SegmentD, {"CA1", "CA2"}, "D"},
    {LampShape::SegmentE, {"CA1", "CA2"}, "E"},
    {LampShape::SegmentF, {"CA1", "CA2"}, "F"},
    {LampShape::SegmentG, {"CA1", "CA2"}, "G"},
    {LampShape::DecimalPoint, {"CA1", "CA2"}, "DP"},
}};

using enum PartKind;

// Sorted by name.
constexpr std::array s_parts{
    PartInfo{"74LS00", "quad 2-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS00">},
    PartInfo{"74LS02", "quad 2-input NOR", "DIP-14", Chip, &CreateGatePackage<"74LS02">},
    PartInfo{"74LS04", "hex inverter", "DIP-14", Chip, &CreateGatePackage<"74LS04">},
    PartInfo{"74LS08", "quad 2-input AND", "DIP-14", Chip, &CreateGatePackage<"74LS08">},
    PartInfo{"74LS107", "dual JK flip-flop, clear", "DIP-14", Chip, &CreateChip<Ls107, "74LS107">},
    PartInfo{"74LS161", "synchronous 4-bit binary counter", "DIP-16", Chip,
             &CreateChip<Ls161, "74LS161">},
    PartInfo{"74LS164", "8-bit serial-in shift register", "DIP-14", Chip,
             &CreateChip<Ls164, "74LS164">},
    PartInfo{"74LS175", "quad D flip-flop, clear", "DIP-16", Chip, &CreateChip<Ls175, "74LS175">},
    PartInfo{"74LS20", "dual 4-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS20">},
    PartInfo{"74LS27", "triple 3-input NOR", "DIP-14", Chip, &CreateGatePackage<"74LS27">},
    PartInfo{"74LS30", "8-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS30">},
    PartInfo{"74LS32", "quad 2-input OR", "DIP-14", Chip, &CreateGatePackage<"74LS32">},
    PartInfo{"74LS47", "BCD to 7-segment decoder, open collector", "DIP-16", Chip,
             &CreateChip<Ls47, "74LS47">},
    PartInfo{"74LS74", "dual D flip-flop, preset and clear", "DIP-14", Chip,
             &CreateChip<Ls74, "74LS74">},
    PartInfo{"74LS86", "quad 2-input XOR", "DIP-14", Chip, &CreateGatePackage<"74LS86">},
    PartInfo{"74LS90", "decade counter", "DIP-14", Chip, &CreateChip<Ls90, "74LS90">},
    PartInfo{"74LS93", "4-bit binary counter", "DIP-14", Chip, &CreateChip<Ls93, "74LS93">},
    PartInfo{"BUTTON", "push button", "TACT-6x3.5", Button, &CreateButton},
    PartInfo{"DIGIT", "red 0.56-inch 7-segment digit, common anode", "DIGIT-0.56", Led,
             &CreateDigit, kDigitLamps},
    PartInfo{"LED", "red 5 mm LED", "LED-5mm", Led, &CreateLed, kLedLamps},
    PartInfo{"OSC", "crystal oscillator", "DIP-14", Chip, &CreateOscillator},
    PartInfo{"PADDLE", "paddle controller connector", "KK-4", Connector, &CreatePaddle},
    PartInfo{"POWER", "5 V power connector", "KK-2", Connector, &CreateConnector<"POWER">},
    PartInfo{"R", "resistor", "axial", Resistor, nullptr},
    PartInfo{"VIDEO", "TTL monitor connector", "KK-3", Connector, &CreateConnector<"VIDEO">},
};

}  // namespace

const PartInfo* FindPart(std::string_view name) {
    for (const PartInfo& part : s_parts) {
        if (part.name == name) {
            return &part;
        }
    }
    return nullptr;
}

std::span<const PartInfo> AllParts() { return s_parts; }

std::optional<double> PowerPinVolts(std::string_view pin) {
    constexpr std::array<std::pair<std::string_view, double>, 2> kVolts{
        {{"VCC", 5.0}, {"GND", 0.0}}};
    for (const auto& [name, volts] : kVolts) {
        if (name == pin) {
            return volts;
        }
    }
    return std::nullopt;
}

}  // namespace bw
