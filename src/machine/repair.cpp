#include "machine/repair.hpp"

#include <vector>

#include "board/kicad_board.hpp"
#include "machine/crack.hpp"

namespace bw {

std::expected<RepairAction, std::string> ParseRepair(std::string_view text) {
    const auto bad = std::unexpected("bad repair '" + std::string(text) + "'");
    if (text.starts_with("swap:")) {
        const std::string_view reference = text.substr(5);
        if (reference.empty()) {
            return bad;
        }
        return SwapRepair{std::string(reference)};
    }
    if (!text.starts_with("mend:")) {
        return bad;
    }
    std::vector<std::string_view> fields;
    for (std::size_t start = 5;;) {
        const std::size_t colon = text.find(':', start);
        fields.push_back(text.substr(start, colon - start));
        if (colon == std::string_view::npos) {
            break;
        }
        start = colon + 1;
    }
    if (fields.size() != 3 || (fields[0] != "F.Cu" && fields[0] != "B.Cu")) {
        return bad;
    }
    const auto x = ParseMillimetres(fields[1]);
    const auto y = ParseMillimetres(fields[2]);
    if (!x || !y) {
        return bad;
    }
    return MendRepair{fields[0] == "F.Cu" ? Layer::Front : Layer::Back, {*x, *y}};
}

std::string FormatRepair(const RepairAction& repair) {
    if (const auto* swap = std::get_if<SwapRepair>(&repair)) {
        return "swap " + swap->reference;
    }
    const auto& mend = std::get<MendRepair>(repair);
    return "mend " + FormatPoint(mend.layer, mend.at);
}

}  // namespace bw
