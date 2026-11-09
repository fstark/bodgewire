#include "machine/repair.hpp"

#include <optional>
#include <vector>

#include "board/kicad_board.hpp"
#include "machine/crack.hpp"

namespace bw {

namespace {

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

// The ':'-separated fields after the prefix.
std::vector<std::string_view> Fields(std::string_view text, std::size_t prefixLength) {
    std::vector<std::string_view> fields;
    for (std::size_t start = prefixLength;;) {
        const std::size_t colon = text.find(':', start);
        fields.push_back(text.substr(start, colon - start));
        if (colon == std::string_view::npos) {
            return fields;
        }
        start = colon + 1;
    }
}

// "F.Cu", "x", "y" in millimetres.
std::optional<std::pair<Layer, Point>> ParseLayerPoint(const std::vector<std::string_view>& f) {
    if (f.size() != 3 || (f[0] != "F.Cu" && f[0] != "B.Cu")) {
        return std::nullopt;
    }
    const auto x = ParseMillimetres(f[1]);
    const auto y = ParseMillimetres(f[2]);
    if (!x || !y) {
        return std::nullopt;
    }
    return std::pair{f[0] == "F.Cu" ? Layer::Front : Layer::Back, Point{*x, *y}};
}

// "U1.14": split at the first '.', both sides non-empty.
std::optional<PinRef> ParsePinRef(std::string_view text) {
    const std::size_t dot = text.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 == text.size()) {
        return std::nullopt;
    }
    return PinRef{std::string(text.substr(0, dot)), std::string(text.substr(dot + 1))};
}

std::string FormatPin(const PinRef& pin) { return pin.reference + "." + pin.pin; }

}  // namespace

std::expected<RepairAction, std::string> ParseRepair(std::string_view text) {
    const auto bad = std::unexpected("bad repair '" + std::string(text) + "'");
    if (text.starts_with("swap:")) {
        const std::string_view reference = text.substr(5);
        if (reference.empty()) {
            return bad;
        }
        return SwapRepair{std::string(reference)};
    }
    if (text.starts_with("resolder:")) {
        const auto pin = ParsePinRef(text.substr(9));
        if (!pin || pin->pin.find(':') != std::string::npos) {
            return bad;
        }
        return ResolderRepair{*pin};
    }
    for (const std::string_view prefix : {"mend:", "cut:"}) {
        if (!text.starts_with(prefix)) {
            continue;
        }
        const auto at = ParseLayerPoint(Fields(text, prefix.size()));
        if (!at) {
            return bad;
        }
        if (prefix == "mend:") {
            return MendRepair{at->first, at->second};
        }
        return CutRepair{at->first, at->second};
    }
    for (const std::string_view prefix : {"wire:", "unwire:", "unbridge:"}) {
        if (!text.starts_with(prefix)) {
            continue;
        }
        const auto fields = Fields(text, prefix.size());
        if (fields.size() != 2 || fields[0] == fields[1]) {
            return bad;
        }
        const auto a = ParsePinRef(fields[0]);
        const auto b = ParsePinRef(fields[1]);
        if (!a || !b) {
            return bad;
        }
        if (prefix == "wire:") {
            return WireRepair{*a, *b};
        }
        if (prefix == "unbridge:") {
            return UnbridgeRepair{*a, *b};
        }
        return UnwireRepair{*a, *b};
    }
    return bad;
}

std::string FormatRepair(const RepairAction& repair) {
    return std::visit(
        Overloaded{
            [](const SwapRepair& r) { return "swap " + r.reference; },
            [](const MendRepair& r) { return "mend " + FormatPoint(r.layer, r.at); },
            [](const CutRepair& r) { return "cut " + FormatPoint(r.layer, r.at); },
            [](const WireRepair& r) { return "wire " + FormatPin(r.a) + " " + FormatPin(r.b); },
            [](const UnwireRepair& r) { return "unwire " + FormatPin(r.a) + " " + FormatPin(r.b); },
            [](const UnbridgeRepair& r) {
                return "unbridge " + FormatPin(r.a) + " " + FormatPin(r.b);
            },
            [](const ResolderRepair& r) { return "resolder " + FormatPin(r.pin); }},
        repair);
}

}  // namespace bw
