#include "parts/catalogue.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "parts/connector.hpp"
#include "parts/digit.hpp"
#include "parts/gate_package.hpp"
#include "parts/keyboard.hpp"
#include "parts/led.hpp"
#include "parts/ls107.hpp"
#include "parts/ls138.hpp"
#include "parts/ls139.hpp"
#include "parts/ls154.hpp"
#include "parts/ls157.hpp"
#include "parts/ls161.hpp"
#include "parts/ls164.hpp"
#include "parts/ls165.hpp"
#include "parts/ls175.hpp"
#include "parts/ls244.hpp"
#include "parts/ls273.hpp"
#include "parts/ls365.hpp"
#include "parts/ls373.hpp"
#include "parts/ls47.hpp"
#include "parts/ls48.hpp"
#include "parts/ls74.hpp"
#include "parts/ls90.hpp"
#include "parts/ls93.hpp"
#include "parts/matrix_keyboard.hpp"
#include "parts/mc3446.hpp"
#include "parts/membrane_keyboard.hpp"
#include "parts/mos6502.hpp"
#include "parts/mos6520.hpp"
#include "parts/mos6522.hpp"
#include "parts/mos6530.hpp"
#include "parts/oscillator.hpp"
#include "parts/paddle.hpp"
#include "parts/power_on_reset.hpp"
#include "parts/ram2114.hpp"
#include "parts/ram6102.hpp"
#include "parts/rom.hpp"
#include "parts/rom_contents.hpp"
#include "parts/shift_register_2518.hpp"
#include "parts/shift_register_2524.hpp"
#include "parts/switch.hpp"
#include "parts/ttl100.hpp"
#include "parts/ttl125.hpp"
#include "parts/ttl145.hpp"
#include "parts/ttl177.hpp"
#include "parts/z80.hpp"
#include "parts/z80_stub.hpp"

namespace bw {

namespace {

template <std::size_t N>
struct PartName {
    constexpr PartName(const char (&text)[N]) { std::copy_n(text, N, chars); }
    constexpr std::string_view view() const { return {chars, N - 1}; }
    char chars[N];
};

using Params = std::map<std::string, std::string, std::less<>>;

// `params` by name, each one of `known` and given at most once: "OSC: unknown parameter 'x'",
// "OSC: freq= given twice".
std::expected<Params, std::string> TakeParams(std::string_view part,
                                              std::span<const PartParam> params,
                                              std::span<const std::string_view> known) {
    Params taken;
    for (const PartParam& param : params) {
        if (std::ranges::find(known, param.name) == known.end()) {
            return std::unexpected(std::string(part) + ": unknown parameter '" + param.name + "'");
        }
        if (!taken.emplace(param.name, param.value).second) {
            return std::unexpected(std::string(part) + ": " + param.name + "= given twice");
        }
    }
    return taken;
}

const std::string* Param(const Params& params, std::string_view name) {
    const auto it = params.find(name);
    return it == params.end() ? nullptr : &it->second;
}

// 74LS145: the 74145 with its own delays.
std::expected<Component*, std::string> CreateLs145(Circuit& circuit, std::string reference,
                                                   std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected("74LS145 takes no parameters");
    }
    return &circuit.add<Ttl145>(std::move(reference), delays::kLs145);
}

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
    const RomSpec& spec = RomSpecOf(name);
    constexpr std::array<std::string_view, 3> kKnown{"contents", "offset", "select"};
    const auto taken = TakeParams(name, params, std::span(kKnown).first(spec.selectable ? 3 : 2));
    if (!taken) {
        return std::unexpected(taken.error());
    }
    const std::string* contents = Param(*taken, "contents");
    const std::string* select = Param(*taken, "select");
    const std::string* offset = Param(*taken, "offset");
    // select= (2316B, 2332): one letter a chip select, CS1 first; H active high, L active low.
    unsigned activeHigh = 0;
    if (spec.selectable) {
        const std::size_t count = spec.enables.size();
        if (select == nullptr) {
            std::string pins;
            for (std::size_t i = 0; i < count; ++i) {
                pins += (i == 0 ? "CS" : " CS") + std::to_string(i + 1);
            }
            return std::unexpected(name + " needs select=<" + pins +
                                   ": L or H each>, e.g. select=" + std::string(count - 1, 'L') +
                                   "H");
        }
        const std::string& letters = *select;
        bool good = letters.size() == spec.enables.size();
        for (std::size_t i = 0; good && i < letters.size(); ++i) {
            good = letters[i] == 'L' || letters[i] == 'H';
            activeHigh |= (letters[i] == 'H' ? 1u : 0u) << i;
        }
        if (!good) {
            const char* number = spec.enables.size() == 2 ? "two" : "three";
            return std::unexpected(name + ": bad select '" + letters + "' (" + number +
                                   " of L or H)");
        }
    }
    if (contents == nullptr) {
        return std::unexpected(name + " needs contents=<file>");
    }
    const std::size_t size = std::size_t{1} << spec.address.size();
    // offset= (P5p): this chip's slice of a larger .rom text, which is read at the full 64K.
    std::size_t from = 0;
    if (offset != nullptr) {
        const std::string& text = *offset;
        const bool hex = text.starts_with('$');
        const std::string digits = hex ? text.substr(1) : text;
        const bool good = !digits.empty() && digits.size() <= 5 &&
                          std::all_of(digits.begin(), digits.end(), [hex](char c) {
                              return hex ? std::isxdigit(static_cast<unsigned char>(c)) != 0
                                         : std::isdigit(static_cast<unsigned char>(c)) != 0;
                          });
        if (!good) {
            return std::unexpected(name + ": bad offset '" + text + "'");
        }
        from = std::stoul(digits, nullptr, hex ? 16 : 10);
        if (from % size != 0 || from + size > 65536) {
            return std::unexpected(name + ": offset must be a multiple of " + std::to_string(size) +
                                   " below 65536");
        }
        std::string extension = std::filesystem::path(*contents).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".bin") {
            return std::unexpected(name + ": offset= is for .rom text files");
        }
    }
    auto words = ReadRomContents(*contents, offset != nullptr ? 65536 : size,
                                 static_cast<int>(spec.data.size()));
    if (!words) {
        return std::unexpected(name + ": " + words.error());
    }
    if (offset != nullptr) {
        words->erase(words->begin(), words->begin() + static_cast<std::ptrdiff_t>(from));
        words->resize(size);
    }
    return &circuit.add<Rom>(std::move(reference), spec, std::move(*words), activeHigh);
}

std::expected<Component*, std::string> CreateMos6530(Circuit& circuit, std::string reference,
                                                     std::span<const PartParam> params) {
    constexpr std::array<std::string_view, 2> kKnown{"mask", "contents"};
    const auto taken = TakeParams("6530", params, kKnown);
    if (!taken) {
        return std::unexpected(taken.error());
    }
    const std::string* mask = Param(*taken, "mask");
    const std::string* contents = Param(*taken, "contents");
    if (mask == nullptr) {
        return std::unexpected("6530 needs mask=002|003");
    }
    const Mos6530Mask* found = FindMos6530Mask(*mask);
    if (found == nullptr) {
        return std::unexpected("6530: unknown mask '" + *mask + "' (002|003)");
    }
    if (contents == nullptr) {
        return std::unexpected("6530 needs contents=<file>");
    }
    auto words = ReadRomContents(*contents, 1024, 8);
    if (!words) {
        return std::unexpected("6530: " + words.error());
    }
    return &circuit.add<Mos6530>(std::move(reference), *found, std::move(*words));
}

std::expected<Component*, std::string> CreateOscillator(Circuit& circuit, std::string reference,
                                                        std::span<const PartParam> params) {
    constexpr std::array<std::string_view, 1> kKnown{"freq"};
    const auto taken = TakeParams("OSC", params, kKnown);
    if (!taken) {
        return std::unexpected(taken.error());
    }
    const std::string* freq = Param(*taken, "freq");
    if (freq == nullptr) {
        return std::unexpected("OSC needs freq=<frequency>");
    }
    const auto period = ParseFrequencyPeriod(*freq);
    if (!period) {
        return std::unexpected(period.error());
    }
    return &circuit.add<Oscillator>(std::move(reference), *period);
}

std::expected<Component*, std::string> CreatePaddle(Circuit& circuit, std::string reference,
                                                    std::span<const PartParam> params) {
    constexpr std::array<std::string_view, 3> kKnown{"min", "max", "reversed"};
    const auto taken = TakeParams("PADDLE", params, kKnown);
    if (!taken) {
        return std::unexpected(taken.error());
    }
    const std::string* shortest = Param(*taken, "min");
    const std::string* longest = Param(*taken, "max");
    const std::string* reversedText = Param(*taken, "reversed");
    if (reversedText && *reversedText != "yes" && *reversedText != "no") {
        return std::unexpected("PADDLE: reversed must be yes or no");
    }
    const bool reversed = reversedText && *reversedText == "yes";
    if (shortest == nullptr || longest == nullptr) {
        return std::unexpected("PADDLE needs min=<duration> and max=<duration>");
    }
    const auto low = ParseDuration(*shortest);
    if (!low) {
        return std::unexpected(low.error());
    }
    const auto high = ParseDuration(*longest);
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
    constexpr std::array<std::string_view, 1> kKnown{"time"};
    const auto taken = TakeParams("POR", params, kKnown);
    if (!taken) {
        return std::unexpected(taken.error());
    }
    const std::string* time = Param(*taken, "time");
    if (time == nullptr) {
        return std::unexpected("POR needs time=<duration>");
    }
    const auto hold = ParseDuration(*time);
    if (!hold) {
        return std::unexpected(hold.error());
    }
    if (*hold < 1'000'000) {
        return std::unexpected("POR: time must be at least 1us");
    }
    return &circuit.add<PowerOnReset>(std::move(reference), *hold);
}

// access=450ns (the 2114, default), 300ns (2114-3) or 200ns (2114-2): the speed grade.
std::expected<Component*, std::string> CreateRam2114(Circuit& circuit, std::string reference,
                                                     std::span<const PartParam> params) {
    constexpr std::array<std::string_view, 1> kKnown{"access"};
    const auto taken = TakeParams("2114", params, kKnown);
    if (!taken) {
        return std::unexpected(taken.error());
    }
    Time access = delays::k2114Address;
    if (const std::string* text = Param(*taken, "access")) {
        const auto time = ParseDuration(*text);
        if (!time || (*time != 450'000 && *time != 300'000 && *time != 200'000)) {
            return std::unexpected(
                "2114: access= is 450ns, 300ns or 200ns (the 2114, 2114-3, 2114-2)");
        }
        access = *time;
    }
    return &circuit.add<Ram2114>(std::move(reference), access);
}

std::expected<Component*, std::string> CreateMatrixKeyboard(Circuit& circuit, std::string reference,
                                                            std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected("BW2001KB takes no parameters");
    }
    return &circuit.add<MatrixKeyboard>(std::move(reference));
}

std::expected<Component*, std::string>
CreateMembraneKeyboard(Circuit& circuit, std::string reference, std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected("BW80KB takes no parameters");
    }
    return &circuit.add<MembraneKeyboard>(std::move(reference));
}

std::expected<Component*, std::string> CreateKeyboard(Circuit& circuit, std::string reference,
                                                      std::span<const PartParam> params) {
    constexpr std::array<std::string_view, 1> kKnown{"strobe"};
    const auto taken = TakeParams("KEYBOARD", params, kKnown);
    if (!taken) {
        return std::unexpected(taken.error());
    }
    Time strobe = kKeyboardStrobe;
    if (const std::string* text = Param(*taken, "strobe")) {
        const auto length = ParseDuration(*text);
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

// BUTTON: `legend=` (1-8 printable characters, not '#', on the silkscreen: P6g) and `pc=`; KEY:
// `legend=` (1-4: it fills a cap) and `pc=`, which defaults to a one-character legend; SLIDE:
// `legend=` (1-8) only (moved by a click, no PC key).
template <PartName Name>
std::expected<Component*, std::string> CreateSwitch(Circuit& circuit, std::string reference,
                                                    std::span<const PartParam> params) {
    const std::string name(Name.view());
    const bool key = name == "KEY";
    const bool slide = name == "SLIDE";
    std::optional<std::string> legend, pc;
    for (const PartParam& param : params) {
        std::optional<std::string>* slot = !slide && param.name == "pc" ? &pc
                                           : param.name == "legend"     ? &legend
                                                                        : nullptr;
        if (slot == nullptr) {
            return std::unexpected(name + (slide ? " takes legend=" : " takes legend= and pc=") +
                                   ", not " + param.name + "=");
        }
        if (slot->has_value()) {
            return std::unexpected(name + ": " + param.name + "= given twice");
        }
        *slot = param.value;
    }
    const std::size_t longest = key ? 4 : 8;
    if (legend &&
        (legend->empty() || legend->size() > longest ||
         !std::ranges::all_of(*legend, [](char c) { return c > ' ' && c < 0x7F && c != '#'; }))) {
        return std::unexpected(std::format(
            "{} legend= takes 1 to {} printable characters, not '{}'", name, longest, *legend));
    }
    if (pc && !IsPcKey(*pc)) {
        return std::unexpected(name + ": unknown PC key '" + *pc +
                               "' (one character, Space, Enter, Tab, Backspace, Escape, Up, "
                               "Down, Left or Right)");
    }
    if (key && !pc && legend && IsPcKey(*legend) && legend->size() == 1) {
        pc = legend;
    }
    std::string pcKey = pc.value_or("");
    if (pcKey.size() == 1) {
        pcKey[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(pcKey[0])));
    }
    const SwitchKind kind = key ? SwitchKind::Key : slide ? SwitchKind::Slide : SwitchKind::Button;
    return &circuit.add<Switch>(std::move(reference), kind, legend.value_or(""), std::move(pcKey));
}

template <DigitCommon Common>
std::expected<Component*, std::string> CreateDigit(Circuit& circuit, std::string reference,
                                                   std::span<const PartParam> params) {
    if (!params.empty()) {
        return std::unexpected(Common == DigitCommon::Anode ? "DIGIT takes no parameters"
                                                            : "DIGITCC takes no parameters");
    }
    return &circuit.add<Digit>(std::move(reference), Common);
}

constexpr std::array<Lamp, 1> kLedLamps{{{LampShape::Round, {"A", ""}, {"K", ""}}}};
constexpr std::array<Lamp, 8> kDigitLamps{{
    {LampShape::SegmentA, {"CA1", "CA2"}, {"A", ""}},
    {LampShape::SegmentB, {"CA1", "CA2"}, {"B", ""}},
    {LampShape::SegmentC, {"CA1", "CA2"}, {"C", ""}},
    {LampShape::SegmentD, {"CA1", "CA2"}, {"D", ""}},
    {LampShape::SegmentE, {"CA1", "CA2"}, {"E", ""}},
    {LampShape::SegmentF, {"CA1", "CA2"}, {"F", ""}},
    {LampShape::SegmentG, {"CA1", "CA2"}, {"G", ""}},
    {LampShape::DecimalPoint, {"CA1", "CA2"}, {"DP", ""}},
}};
constexpr std::array<Lamp, 8> kDigitCcLamps{{
    {LampShape::SegmentA, {"A", ""}, {"CC1", "CC2"}},
    {LampShape::SegmentB, {"B", ""}, {"CC1", "CC2"}},
    {LampShape::SegmentC, {"C", ""}, {"CC1", "CC2"}},
    {LampShape::SegmentD, {"D", ""}, {"CC1", "CC2"}},
    {LampShape::SegmentE, {"E", ""}, {"CC1", "CC2"}},
    {LampShape::SegmentF, {"F", ""}, {"CC1", "CC2"}},
    {LampShape::SegmentG, {"G", ""}, {"CC1", "CC2"}},
    {LampShape::DecimalPoint, {"DP", ""}, {"CC1", "CC2"}},
}};

using enum PartKind;

// Sorted by name.
// Examples for PartPinNames.
const std::array<PartParam, 1> kOscSample{{{"freq", "1MHz"}}};
const std::array<PartParam, 1> kPorSample{{{"time", "1ms"}}};
const std::array<PartParam, 2> kPaddleSample{{{"min", "30us"}, {"max", "9.93ms"}}};

constexpr std::array s_parts{
    PartInfo{"2114", "1K x 4 static RAM", "DIP-18", Chip, &CreateRam2114},
    PartInfo{"2316B", "2K x 8 mask ROM, mask-set chip selects", "DIP-24", Chip,
             &CreateRom<"2316B">},
    PartInfo{"2332", "4K x 8 mask ROM, mask-set chip selects", "DIP-24", Chip, &CreateRom<"2332">},
    PartInfo{"2513", "64-character 5 x 7 character generator ROM", "DIP-24", Chip,
             &CreateRom<"2513">},
    PartInfo{"2518", "hex 32-bit static shift register", "DIP-16", Chip,
             &CreateChip<ShiftRegister2518, "2518">},
    PartInfo{"2524", "512-bit dynamic recirculating shift register", "DIP-8", Chip,
             &CreateChip<ShiftRegister2524, "2524">},
    PartInfo{"2716", "2K x 8 EPROM", "DIP-24", Chip, &CreateRom<"2716">},
    PartInfo{"6102", "1K x 1 static RAM", "DIP-16", Chip, &CreateChip<Ram6102, "6102">},
    PartInfo{"6502", "8-bit microprocessor (NMOS)", "DIP-40", Chip, &CreateChip<Mos6502, "6502">},
    PartInfo{"6520", "peripheral interface adapter (PIA)", "DIP-40", Chip,
             &CreateChip<Mos6520, "6520">},
    PartInfo{"6522", "versatile interface adapter (VIA)", "DIP-40", Chip,
             &CreateChip<Mos6522, "6522">},
    PartInfo{"6530", "ROM, RAM, I/O and timer (KIM mask 002 or 003)", "DIP-40", Chip,
             &CreateMos6530},
    PartInfo{"74100", "8-bit bistable latch", "DIP-24", Chip, &CreateChip<Ttl100, "74100">},
    PartInfo{"74125", "quad bus buffer, tri-state", "DIP-14", Chip, &CreateChip<Ttl125, "74125">},
    PartInfo{"74145", "BCD-to-decimal decoder/driver, open collector", "DIP-16", Chip,
             &CreateChip<Ttl145, "74145">},
    PartInfo{"7417", "hex buffer, open collector", "DIP-14", Chip, &CreateGatePackage<"7417">},
    PartInfo{"74177", "presettable binary counter/latch", "DIP-14", Chip,
             &CreateChip<Ttl177, "74177">},
    PartInfo{"74LS00", "quad 2-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS00">},
    PartInfo{"74LS02", "quad 2-input NOR", "DIP-14", Chip, &CreateGatePackage<"74LS02">},
    PartInfo{"74LS04", "hex inverter", "DIP-14", Chip, &CreateGatePackage<"74LS04">},
    PartInfo{"74LS05", "hex inverter, open collector", "DIP-14", Chip,
             &CreateGatePackage<"74LS05">},
    PartInfo{"74LS08", "quad 2-input AND", "DIP-14", Chip, &CreateGatePackage<"74LS08">},
    PartInfo{"74LS10", "triple 3-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS10">},
    PartInfo{"74LS107", "dual JK flip-flop, clear", "DIP-14", Chip, &CreateChip<Ls107, "74LS107">},
    PartInfo{"74LS138", "3-to-8 line decoder", "DIP-16", Chip, &CreateChip<Ls138, "74LS138">},
    PartInfo{"74LS139", "dual 2-to-4 line decoder", "DIP-16", Chip, &CreateChip<Ls139, "74LS139">},
    PartInfo{"74LS145", "BCD-to-decimal decoder/driver, open collector", "DIP-16", Chip,
             &CreateLs145},
    PartInfo{"74LS154", "4-to-16 line decoder", "DIP-24", Chip, &CreateChip<Ls154, "74LS154">},
    PartInfo{"74LS157", "quad 2-to-1 multiplexer", "DIP-16", Chip, &CreateChip<Ls157, "74LS157">},
    PartInfo{"74LS161", "synchronous 4-bit binary counter", "DIP-16", Chip,
             &CreateChip<Ls161, "74LS161">},
    PartInfo{"74LS164", "8-bit serial-in shift register", "DIP-14", Chip,
             &CreateChip<Ls164, "74LS164">},
    PartInfo{"74LS165", "8-bit parallel-in shift register", "DIP-16", Chip,
             &CreateChip<Ls165, "74LS165">},
    PartInfo{"74LS175", "quad D flip-flop, clear", "DIP-16", Chip, &CreateChip<Ls175, "74LS175">},
    PartInfo{"74LS20", "dual 4-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS20">},
    PartInfo{"74LS21", "dual 4-input AND", "DIP-14", Chip, &CreateGatePackage<"74LS21">},
    PartInfo{"74LS244", "octal buffer, tri-state", "DIP-20", Chip, &CreateChip<Ls244, "74LS244">},
    PartInfo{"74LS27", "triple 3-input NOR", "DIP-14", Chip, &CreateGatePackage<"74LS27">},
    PartInfo{"74LS273", "octal D flip-flop, clear", "DIP-20", Chip, &CreateChip<Ls273, "74LS273">},
    PartInfo{"74LS30", "8-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS30">},
    PartInfo{"74LS32", "quad 2-input OR", "DIP-14", Chip, &CreateGatePackage<"74LS32">},
    PartInfo{"74LS365", "hex buffer, tri-state, two enables", "DIP-16", Chip,
             &CreateChip<Ls365, "74LS365">},
    PartInfo{"74LS373", "octal transparent latch, tri-state", "DIP-20", Chip,
             &CreateChip<Ls373, "74LS373">},
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
    PartInfo{"C", "capacitor", "disc", Capacitor, nullptr},
    PartInfo{"CASSETTE", "cassette port connector", "HDR-1x06", Connector,
             &CreateConnector<"CASSETTE">},
    PartInfo{"DIGIT", "red 0.56-inch 7-segment digit, common anode", "DIGIT-0.56", Led,
             &CreateDigit<DigitCommon::Anode>, kDigitLamps},
    PartInfo{"DIGITCC", "red 0.56-inch 7-segment digit, common cathode", "DIGIT-0.56", Led,
             &CreateDigit<DigitCommon::Cathode>, kDigitCcLamps},
    PartInfo{"EXPANSION", "expansion bus edge connector (2 x 23)", "HDR-2x23", Connector,
             &CreateConnector<"EXPANSION">},
    PartInfo{"IEEE488", "IEEE-488 edge connector", "HDR-2x12", Connector,
             &CreateConnector<"IEEE488">},
    PartInfo{"KEY", "key switch", "MX-1U", Switch, &CreateSwitch<"KEY">},
    PartInfo{"KEYBOARD", "ASCII keyboard connector", "KK-10", Connector, &CreateKeyboard},
    PartInfo{"LED", "red 5 mm LED", "LED-5mm", Led, &CreateLed, kLedLamps},
    PartInfo{"BW2001KB", "BW-2001 keyboard (10 x 8 key matrix)", "HDR-1x20", Connector,
             &CreateMatrixKeyboard},
    PartInfo{"MC3446", "quad IEEE-488 bus transceiver", "DIP-16", Chip,
             &CreateChip<Mc3446, "MC3446">},
    PartInfo{"BW80KB", "BW-80 keyboard (8 x 5 membrane matrix)", "HDR-1x13", Connector,
             &CreateMembraneKeyboard},
    PartInfo{"MONITOR", "monitor connector, separate drives", "HDR-1x07", Connector,
             &CreateConnector<"MONITOR">},
    PartInfo{"OSC", "crystal oscillator", "DIP-14", Chip, &CreateOscillator, {}, kOscSample},
    PartInfo{"PADDLE",
             "paddle controller connector",
             "KK-4",
             Connector,
             &CreatePaddle,
             {},
             kPaddleSample},
    PartInfo{"POR", "power-on reset", "DIP-8", Chip, &CreatePowerOnReset, {}, kPorSample},
    PartInfo{"POWER", "5 V power connector", "KK-2", Connector, &CreateConnector<"POWER">},
    PartInfo{"R", "resistor", "axial", Resistor, nullptr},
    PartInfo{"SLIDE", "slide switch", "SLIDE-1", Switch, &CreateSwitch<"SLIDE">},
    PartInfo{"USERPORT", "user port edge connector", "HDR-2x12", Connector,
             &CreateConnector<"USERPORT">},
    PartInfo{"VIDEO", "TTL monitor connector", "KK-3", Connector, &CreateConnector<"VIDEO">},
    PartInfo{"Z80", "8-bit microprocessor (NMOS)", "DIP-40", Chip, &CreateChip<Z80, "Z80">},
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

std::vector<std::string> PartPinNames(const PartInfo& part) {
    if (!part.create) {
        return {};
    }
    if (const RomSpec* rom = FindRomSpec(part.name)) {  // a ROM needs a contents file to build
        return {rom->names.begin(), rom->names.end()};
    }
    std::vector<std::string> names;
    if (part.name == "6530") {  // so does a 6530
        for (const PinSpec& pin : Mos6530::Pins()) {
            names.push_back(pin.name);
        }
        return names;
    }
    Circuit circuit;
    if (const auto component = part.create(circuit, "X", part.sample)) {
        for (const PinSpec& spec : (*component)->pins()) {
            names.push_back(spec.name);
        }
    }
    return names;
}

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
