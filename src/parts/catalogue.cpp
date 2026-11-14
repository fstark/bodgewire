#include "parts/catalogue.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <optional>
#include <utility>

#include "parts/connector.hpp"
#include "parts/digit.hpp"
#include "parts/gate_package.hpp"
#include "parts/keyboard.hpp"
#include "parts/led.hpp"
#include "parts/ls107.hpp"
#include "parts/ls138.hpp"
#include "parts/ls157.hpp"
#include "parts/ls161.hpp"
#include "parts/ls164.hpp"
#include "parts/ls165.hpp"
#include "parts/ls175.hpp"
#include "parts/ls244.hpp"
#include "parts/ls273.hpp"
#include "parts/ls47.hpp"
#include "parts/ls48.hpp"
#include "parts/ls74.hpp"
#include "parts/ls90.hpp"
#include "parts/ls93.hpp"
#include "parts/mos6502.hpp"
#include "parts/oscillator.hpp"
#include "parts/paddle.hpp"
#include "parts/power_on_reset.hpp"
#include "parts/ram2114.hpp"
#include "parts/rom.hpp"
#include "parts/rom_contents.hpp"
#include "parts/shift_register_2518.hpp"
#include "parts/shift_register_2524.hpp"
#include "parts/switch.hpp"
#include "parts/z80_stub.hpp"

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

template <PartName Name>
std::expected<Component*, std::string> CreateRom(Circuit& circuit, std::string reference,
                                                 std::span<const PartParam> params) {
    const std::string name(Name.view());
    const PartParam* contents = nullptr;
    for (const PartParam& param : params) {
        if (param.name != "contents") {
            return std::unexpected(name + ": unknown parameter '" + param.name + "'");
        }
        contents = &param;
    }
    if (contents == nullptr) {
        return std::unexpected(name + " needs contents=<file>");
    }
    const RomSpec& spec = RomSpecOf(name);
    auto words = ReadRomContents(contents->value, std::size_t{1} << spec.address.size(),
                                 static_cast<int>(spec.data.size()));
    if (!words) {
        return std::unexpected(name + ": " + words.error());
    }
    return &circuit.add<Rom>(std::move(reference), spec, std::move(*words));
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
    bool reversed = false;
    for (const PartParam& param : params) {
        if (param.name == "min") {
            shortest = &param;
        } else if (param.name == "max") {
            longest = &param;
        } else if (param.name == "reversed") {
            if (param.value != "yes" && param.value != "no") {
                return std::unexpected("PADDLE: reversed must be yes or no");
            }
            reversed = param.value == "yes";
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
    return &circuit.add<Paddle>(std::move(reference), *low, *high, reversed);
}

std::expected<Component*, std::string> CreatePowerOnReset(Circuit& circuit, std::string reference,
                                                          std::span<const PartParam> params) {
    const PartParam* time = nullptr;
    for (const PartParam& param : params) {
        if (param.name != "time") {
            return std::unexpected("POR: unknown parameter '" + param.name + "'");
        }
        time = &param;
    }
    if (time == nullptr) {
        return std::unexpected("POR needs time=<duration>");
    }
    const auto hold = DurationFromText(time->value);
    if (!hold) {
        return std::unexpected(hold.error());
    }
    if (*hold < 1'000'000) {
        return std::unexpected("POR: time must be at least 1us");
    }
    return &circuit.add<PowerOnReset>(std::move(reference), *hold);
}

std::expected<Component*, std::string> CreateKeyboard(Circuit& circuit, std::string reference,
                                                      std::span<const PartParam> params) {
    Time strobe = kKeyboardStrobe;
    for (const PartParam& param : params) {
        if (param.name != "strobe") {
            return std::unexpected("KEYBOARD: unknown parameter '" + param.name + "'");
        }
        const auto length = DurationFromText(param.value);
        if (!length) {
            return std::unexpected(length.error());
        }
        if (*length < 1'000'000) {
            return std::unexpected("KEYBOARD: strobe must be at least 1us");
        }
        strobe = *length;
    }
    return &circuit.add<Keyboard>(std::move(reference), strobe);
}

std::expected<Component*, std::string> CreateLed(Circuit& circuit, std::string reference,
                                                 std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected("LED takes no parameters");
    }
    return &circuit.add<Led>(std::move(reference));
}

// BUTTON: `pc=`; KEY: `legend=` (1-4 printable characters, not '#') and `pc=`, which defaults
// to a one-character legend.
template <PartName Name>
std::expected<Component*, std::string> CreateSwitch(Circuit& circuit, std::string reference,
                                                    std::span<const PartParam> params) {
    const std::string name(Name.view());
    const bool key = name == "KEY";
    std::optional<std::string> legend, pc;
    for (const PartParam& param : params) {
        std::optional<std::string>* slot = param.name == "pc"              ? &pc
                                           : key && param.name == "legend" ? &legend
                                                                           : nullptr;
        if (slot == nullptr) {
            return std::unexpected(name + (key ? " takes legend= and pc=" : " takes pc=") +
                                   ", not " + param.name + "=");
        }
        if (slot->has_value()) {
            return std::unexpected(name + ": " + param.name + "= given twice");
        }
        *slot = param.value;
    }
    if (legend &&
        (legend->empty() || legend->size() > 4 ||
         !std::ranges::all_of(*legend, [](char c) { return c > ' ' && c < 0x7F && c != '#'; }))) {
        return std::unexpected("KEY legend= takes 1 to 4 printable characters, not '" + *legend +
                               "'");
    }
    if (pc && !IsPcKey(*pc)) {
        return std::unexpected(name + ": unknown PC key '" + *pc +
                               "' (one character, Space, Enter, Tab, Backspace, Escape, Up, "
                               "Down, Left or Right)");
    }
    if (!pc && legend && IsPcKey(*legend) && legend->size() == 1) {
        pc = legend;
    }
    std::string pcKey = pc.value_or("");
    if (pcKey.size() == 1) {
        pcKey[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(pcKey[0])));
    }
    return &circuit.add<Switch>(std::move(reference), legend.value_or(""), std::move(pcKey));
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
    PartInfo{"2114", "1K x 4 static RAM", "DIP-18", Chip, &CreateChip<Ram2114, "2114">},
    PartInfo{"2513", "64-character 5 x 7 character generator ROM", "DIP-24", Chip,
             &CreateRom<"2513">},
    PartInfo{"2518", "hex 32-bit static shift register", "DIP-16", Chip,
             &CreateChip<ShiftRegister2518, "2518">},
    PartInfo{"2524", "512-bit dynamic recirculating shift register", "DIP-8", Chip,
             &CreateChip<ShiftRegister2524, "2524">},
    PartInfo{"2716", "2K x 8 EPROM", "DIP-24", Chip, &CreateRom<"2716">},
    PartInfo{"6502", "8-bit microprocessor (NMOS)", "DIP-40", Chip, &CreateChip<Mos6502, "6502">},
    PartInfo{"74LS00", "quad 2-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS00">},
    PartInfo{"74LS02", "quad 2-input NOR", "DIP-14", Chip, &CreateGatePackage<"74LS02">},
    PartInfo{"74LS04", "hex inverter", "DIP-14", Chip, &CreateGatePackage<"74LS04">},
    PartInfo{"74LS08", "quad 2-input AND", "DIP-14", Chip, &CreateGatePackage<"74LS08">},
    PartInfo{"74LS107", "dual JK flip-flop, clear", "DIP-14", Chip, &CreateChip<Ls107, "74LS107">},
    PartInfo{"74LS138", "3-to-8 line decoder", "DIP-16", Chip, &CreateChip<Ls138, "74LS138">},
    PartInfo{"74LS157", "quad 2-to-1 multiplexer", "DIP-16", Chip, &CreateChip<Ls157, "74LS157">},
    PartInfo{"74LS161", "synchronous 4-bit binary counter", "DIP-16", Chip,
             &CreateChip<Ls161, "74LS161">},
    PartInfo{"74LS164", "8-bit serial-in shift register", "DIP-14", Chip,
             &CreateChip<Ls164, "74LS164">},
    PartInfo{"74LS165", "8-bit parallel-in shift register", "DIP-16", Chip,
             &CreateChip<Ls165, "74LS165">},
    PartInfo{"74LS175", "quad D flip-flop, clear", "DIP-16", Chip, &CreateChip<Ls175, "74LS175">},
    PartInfo{"74LS20", "dual 4-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS20">},
    PartInfo{"74LS244", "octal buffer, tri-state", "DIP-20", Chip, &CreateChip<Ls244, "74LS244">},
    PartInfo{"74LS27", "triple 3-input NOR", "DIP-14", Chip, &CreateGatePackage<"74LS27">},
    PartInfo{"74LS273", "octal D flip-flop, clear", "DIP-20", Chip, &CreateChip<Ls273, "74LS273">},
    PartInfo{"74LS30", "8-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS30">},
    PartInfo{"74LS32", "quad 2-input OR", "DIP-14", Chip, &CreateGatePackage<"74LS32">},
    PartInfo{"74LS47", "BCD to 7-segment decoder, open collector", "DIP-16", Chip,
             &CreateChip<Ls47, "74LS47">},
    PartInfo{"74LS48", "BCD to 7-segment decoder, active-high outputs", "DIP-16", Chip,
             &CreateChip<Ls48, "74LS48">},
    PartInfo{"74LS74", "dual D flip-flop, preset and clear", "DIP-14", Chip,
             &CreateChip<Ls74, "74LS74">},
    PartInfo{"74LS86", "quad 2-input XOR", "DIP-14", Chip, &CreateGatePackage<"74LS86">},
    PartInfo{"74LS90", "decade counter", "DIP-14", Chip, &CreateChip<Ls90, "74LS90">},
    PartInfo{"74LS93", "4-bit binary counter", "DIP-14", Chip, &CreateChip<Ls93, "74LS93">},
    PartInfo{"BUTTON", "push button", "TACT-6x3.5", Switch, &CreateSwitch<"BUTTON">},
    PartInfo{"DIGIT", "red 0.56-inch 7-segment digit, common anode", "DIGIT-0.56", Led,
             &CreateDigit, kDigitLamps},
    PartInfo{"KEY", "key switch", "MX-1U", Switch, &CreateSwitch<"KEY">},
    PartInfo{"KEYBOARD", "ASCII keyboard connector", "KK-10", Connector, &CreateKeyboard},
    PartInfo{"LED", "red 5 mm LED", "LED-5mm", Led, &CreateLed, kLedLamps},
    PartInfo{"OSC", "crystal oscillator", "DIP-14", Chip, &CreateOscillator},
    PartInfo{"PADDLE", "paddle controller connector", "KK-4", Connector, &CreatePaddle},
    PartInfo{"POR", "power-on reset", "DIP-8", Chip, &CreatePowerOnReset},
    PartInfo{"POWER", "5 V power connector", "KK-2", Connector, &CreateConnector<"POWER">},
    PartInfo{"R", "resistor", "axial", Resistor, nullptr},
    PartInfo{"VIDEO", "TTL monitor connector", "KK-3", Connector, &CreateConnector<"VIDEO">},
    PartInfo{"Z80STUB", "Z80 stub playing a ZX80 frame (benchmark)", "DIP-40", Chip,
             &CreateChip<Z80Stub, "Z80STUB">},
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
    constexpr std::array<std::pair<std::string_view, double>, 3> kVolts{
        {{"VCC", 5.0}, {"GND", 0.0}, {"GND2", 0.0}}};
    for (const auto& [name, volts] : kVolts) {
        if (name == pin) {
            return volts;
        }
    }
    return std::nullopt;
}

std::string_view PowerPinRail(std::string_view pin) { return pin == "GND2" ? "GND" : pin; }

}  // namespace bw
