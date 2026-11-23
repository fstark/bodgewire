#include "machine/repair.hpp"

#include <optional>
#include <vector>

#include "board/kicad_board.hpp"
#include "machine/crack.hpp"
#include "text/text.hpp"

namespace bw {

namespace {

// "F.Cu", "x", "y" in millimetres.
std::optional<std::pair<Layer, Point>> ParseLayerPoint(const std::vector<std::string_view>& f) {
    const auto layer = f.size() == 3 ? ParseLayer(f[0]) : std::nullopt;
    const auto x = layer ? ParseMillimetres(f[1]) : std::nullopt;
    const auto y = layer ? ParseMillimetres(f[2]) : std::nullopt;
    if (!x || !y) {
        return std::nullopt;
    }
    return std::pair{*layer, Point{*x, *y}};
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
        const auto at = ParseLayerPoint(Split(text.substr(prefix.size()), ':'));
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
        const auto fields = Split(text.substr(prefix.size()), ':');
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
