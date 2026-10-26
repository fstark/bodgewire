#include "parts/catalogue.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include "parts/gate_package.hpp"
#include "parts/ls161.hpp"
#include "parts/ls74.hpp"

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

using enum PartKind;

// Sorted by name.
constexpr std::array s_parts{
    PartInfo{"74LS00", "quad 2-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS00">},
    PartInfo{"74LS02", "quad 2-input NOR", "DIP-14", Chip, &CreateGatePackage<"74LS02">},
    PartInfo{"74LS04", "hex inverter", "DIP-14", Chip, &CreateGatePackage<"74LS04">},
    PartInfo{"74LS08", "quad 2-input AND", "DIP-14", Chip, &CreateGatePackage<"74LS08">},
    PartInfo{"74LS161", "synchronous 4-bit binary counter", "DIP-16", Chip,
             &CreateChip<Ls161, "74LS161">},
    PartInfo{"74LS20", "dual 4-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS20">},
    PartInfo{"74LS30", "8-input NAND", "DIP-14", Chip, &CreateGatePackage<"74LS30">},
    PartInfo{"74LS32", "quad 2-input OR", "DIP-14", Chip, &CreateGatePackage<"74LS32">},
    PartInfo{"74LS74", "dual D flip-flop, preset and clear", "DIP-14", Chip,
             &CreateChip<Ls74, "74LS74">},
    PartInfo{"74LS86", "quad 2-input XOR", "DIP-14", Chip, &CreateGatePackage<"74LS86">},
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

}  // namespace bw
