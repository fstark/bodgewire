#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "board/board.hpp"
#include "netlist/builder.hpp"

namespace bw {

struct Bounds {
    Point min, max;  // nm
};

enum class HitKind : std::uint8_t { Pad, Via, Trace, Component };

struct Hit {
    HitKind kind;
    std::size_t component = 0;  // Pad, Component: index in board().components
    std::size_t index = 0;      // Pad: pad within the Component; Via, Trace: index in board()
    bool operator==(const Hit&) const = default;
};

// The drawn Board (with cracks) labelled from the design Netlist and the fault-free Copper:
// nothing here depends on Faults.
class BoardScene {
public:
    BoardScene(Board drawn, const Board& pristine, const LoadedCircuit& design);

    const Board& board() const { return board_; }
    std::span<const Point> outline() const { return outline_; }  // closed, arcs flattened
    std::span<const Point> courtyard(std::size_t component) const { return courtyards_[component]; }
    Bounds bounds() const { return bounds_; }  // of the outline

    std::optional<NetId> padNet(std::size_t component, std::size_t pad) const {
        return padNets_[component][pad];
    }
    std::optional<NetId> viaNet(std::size_t via) const { return viaNets_[via]; }
    std::optional<NetId> traceNet(std::size_t trace) const { return traceNets_[trace]; }
    std::optional<NetId> netOf(const Hit& hit) const;  // Component → nullopt
    std::string_view netName(NetId net) const;

    std::optional<Hit> hitTest(Point at, Layer active) const;
    std::string hoverText(const Hit& hit) const;  // "U1 pin 14 QA · H0", "H0", "U5 74LS161"

private:
    struct ComponentInfo {
        std::string value;                  // LoadedPart::value: "74LS161", "4k7"
        std::vector<std::string> pinNames;  // [pad], "" when unknown (resistors)
    };

    Board board_;
    std::vector<Point> outline_;
    Bounds bounds_{};
    std::vector<std::vector<Point>> courtyards_;              // [component], fallback filled in
    std::vector<std::vector<std::optional<NetId>>> padNets_;  // [component][pad]
    std::vector<std::optional<NetId>> viaNets_, traceNets_;
    std::vector<std::string> netNames_;  // [NetId]
    std::vector<ComponentInfo> info_;    // [component]
};

}  // namespace bw
