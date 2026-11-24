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

// True when `at` is inside the closed polygon `outline` (even-odd): on the Board, for the grease
// pencil and the iron (P6o).
bool InsideOutline(std::span<const Point> outline, Point at);

enum class HitKind : std::uint8_t { Pad, Via, Trace, Component };

struct Hit {
    HitKind kind;
    std::size_t component = 0;  // Pad, Component: index in board().components
    std::size_t index = 0;      // Pad: pad within the Component; Via, Trace: index in board()
    bool operator==(const Hit&) const = default;
};

enum class LeadKind : std::uint8_t { Held, Attached };  // probe, Test Leads / Clips, Hooks
bool AcceptsLead(LeadKind kind, HitKind hit);           // Held: Pad, Via, Trace; Attached: Pad

struct PickReach {
    std::int64_t pad = 0;     // nm: 12 px at the current zoom
    std::int64_t copper = 0;  // nm: 10 px at the current zoom
    // A chip's epoxy body covers the vias and Traces under it: inside it (off its pads) the
    // pick is the chip (the bench's hover, but with Option held).
    bool bodies = false;
    // A pad wins only under the cursor; off it, a Trace or via within `copper` before the pad
    // within half its pitch (the cutters, the iron: the copper between two pins).
    bool copperFirst = false;
};

struct Pick {
    Hit hit;
    Layer layer;    // the picked Trace's Layer; the Active Layer for pads, vias, Components
    Point contact;  // pad/via centre, nearest centreline point of a Trace, else the cursor
};

struct ScenePad {
    std::size_t component;  // index in board().components
    std::size_t pad;        // pad within the Component
    bool operator==(const ScenePad&) const = default;
};

// What a link or a panel points at on the Board: one part (and maybe its pad), a run of parts
// (a manual's `U1`–`U5`) or a Net (a manual's `C19`, P6i).
struct BoardTarget {
    std::vector<std::size_t> components;  // one part, or a run's parts; empty for a Net
    std::optional<std::size_t> pad;       // with one part
    std::optional<NetId> net;             // a Net link
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
    // Pads of the design Net's Drivers (outputs, pull and series resistors, supply), in DescribeNet
    // order.
    std::span<const ScenePad> drivers(NetId net) const;

    // Snapping (S8): pads within reach.pad or half their pitch, then (reach.bodies) a chip's body,
    // then vias, then Traces within reach.copper of their edge (Active Layer first), then
    // courtyards.
    std::optional<Pick> pick(Point at, Layer active, PickReach reach) const;
    // Component whose pad covers `at`, else whose courtyard contains it.
    std::optional<std::size_t> componentAt(Point at) const;
    // Component whose courtyard contains `at` while none of its pads covers it: its body.
    std::optional<std::size_t> bodyAt(Point at) const;
    // The chip whose epoxy body (BodyOf, as drawn: not the oscillator's can) covers `at`.
    std::optional<std::size_t> chipBodyAt(Point at) const;
    std::string hoverText(const Hit& hit) const;  // "U1 pin 14 QA · H0", "H0", "U5 74LS161"

    std::string_view partName(std::size_t component) const;  // "74LS20", "R", "" if unknown
    // A resistor's value ("4k7"), else the Part name; "" if unknown.
    std::string_view value(std::size_t component) const { return info_[component].value; }
    // What is printed on the part beside its name ("1MHz" on an oscillator); "" if nothing.
    std::string_view marking(std::size_t component) const { return info_[component].marking; }
    std::string_view pinName(std::size_t component, std::size_t pad) const;  // "" if unknown
    bool powerPin(std::size_t component, std::size_t pad) const;             // PinKind::Power
    bool chip(std::size_t component) const;  // a PartKind::Chip of the design
    // A Part with lamps (an LED, a digit): what the launcher frames (P7b).
    bool lamp(std::size_t component) const;
    std::optional<std::size_t> componentIndex(std::string_view reference) const;
    // What a pointed part is wired to, for drawing it: per pad, whether its Net reaches another
    // pad; the Nets that do and are not rails (they glow).
    struct Wiring {
        std::vector<bool> used;      // [pad]
        std::vector<NetId> glowing;  // distinct, in pad order
    };
    Wiring wiring(std::size_t component) const;
    // What framing `target` shows: a Net's copper (pads, Traces, vias), a pad and `padMargin`
    // around it, or the parts' courtyards; nullopt when there is nothing (a Net off the Board).
    std::optional<Bounds> boundsOf(const BoardTarget& target, std::int64_t padMargin) const;
    // The narrower side of the part's smallest pad, nm; 0 without pads.
    std::int64_t smallestPad(std::size_t component) const;
    // The Nets a power pin is on (VCC, GND...): drawn as rails, never glowing.
    std::span<const NetId> rails() const { return rails_; }
    // Pad by number, else by pin name.
    std::optional<std::size_t> padIndex(std::size_t component, std::string_view pin) const;

private:
    struct ComponentInfo {
        std::string value;                  // LoadedPart::value: "74LS161", "4k7"
        std::string marking;                // LoadedPart::marking: "1MHz"
        const PartInfo* part = nullptr;     // LoadedPart::info
        std::vector<std::string> pinNames;  // [pad], "" when unknown (resistors)
        std::vector<bool> powerPins;        // [pad]
        std::int64_t halfPitch = 0;         // nm: half the smallest pad spacing, 0 for one pad
    };

    Board board_;
    std::vector<Point> outline_;
    Bounds bounds_{};
    std::vector<std::vector<Point>> courtyards_;              // [component], fallback filled in
    std::vector<std::vector<std::optional<NetId>>> padNets_;  // [component][pad]
    std::vector<std::uint32_t> netPads_;                      // [NetId]: pads on it
    std::vector<NetId> rails_;
    std::vector<std::optional<NetId>> viaNets_, traceNets_;
    std::vector<std::string> netNames_;              // [NetId]
    std::vector<std::vector<ScenePad>> driverPads_;  // [NetId]
    std::vector<ComponentInfo> info_;                // [component]
};

// "leg" (F.Cu) or "pad" (B.Cu) for a chip pad: what a touch there reads; "" for any other Hit.
std::string_view ContactSide(const BoardScene& scene, const Hit& hit, Layer layer);
// "U1.14 QA leg", "R1.1", "Trace", "via"; `layer` the Active Layer when placed.
std::string LeadLabel(const BoardScene& scene, const Hit& hit, Layer layer);

}  // namespace bw
