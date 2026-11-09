#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <variant>

#include "board/board.hpp"
#include "netlist/parser.hpp"

namespace bw {

struct SwapRepair {
    std::string reference;
};

struct MendRepair {
    Layer layer;
    Point at;  // nm
};

struct CutRepair {
    Layer layer;
    Point at;  // nm
};

struct WireRepair {  // also names the wire an UnwireRepair removes
    PinRef a, b;     // pin by number or name
};

struct UnwireRepair {
    PinRef a, b;
};

struct UnbridgeRepair {
    PinRef a, b;
};

struct ResolderRepair {
    PinRef pin;  // by number or name
};

using RepairAction = std::variant<SwapRepair, MendRepair, CutRepair, WireRepair, UnwireRepair,
                                  UnbridgeRepair, ResolderRepair>;

// "swap:U3", "mend:F.Cu:52.30:41.00", "cut:F.Cu:52.30:41.00", "wire:U1.14:U13.6",
// "unwire:U1.QA:U13.6", "unbridge:U1.7:U1.8", "resolder:U15.13"; error "bad repair '...'".
std::expected<RepairAction, std::string> ParseRepair(std::string_view text);
// Repair log line: "swap U3", "mend F.Cu 52.30 41.00", "cut F.Cu 52.30 41.00",
// "wire U1.14 U13.6", "unwire U1.14 U13.6", "unbridge U1.7 U1.8", "resolder U15.13".
std::string FormatRepair(const RepairAction& repair);

}  // namespace bw
