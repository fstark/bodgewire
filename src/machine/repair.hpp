#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <variant>

#include "board/board.hpp"

namespace bw {

struct SwapRepair {
    std::string reference;
};

struct MendRepair {
    Layer layer;
    Point at;  // nm
};

using RepairAction = std::variant<SwapRepair, MendRepair>;

// "swap:U3", "mend:F.Cu:52.30:41.00"; error "bad repair '...'".
std::expected<RepairAction, std::string> ParseRepair(std::string_view text);
// Repair log line: "swap U3", "mend F.Cu 52.30 41.00".
std::string FormatRepair(const RepairAction& repair);

}  // namespace bw
