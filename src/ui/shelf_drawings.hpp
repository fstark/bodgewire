#pragma once

#include <cstdint>

#include <imgui.h>

#include "instruments/logic_probe.hpp"

namespace bw {

// Everything on the shelf (P6c), left to right: controls, the Board's tools, equipment.
enum class Item : std::uint8_t {
    Power,
    Run,
    Speed,
    Layer,
    Fit,
    Pencil,  // the grease pencil (P6h), the first of the Board's tools
    Probe,
    Puller,
    Iron,
    Cutters,
    Wire,
    Tv,
    Keyboard,
    Analyzer,
    Scope,
    Meter,
    Manual,
    Knob,
    Brief  // the Puzzle brief, a post-it on the Board
};

struct ItemLook {
    float alpha = 1.0f;     // 0.35 grayed
    bool lit = false;       // equipment out, power on, running
    ProbeLamps lamps{};     // Probe only
    bool front = true;      // Layer only
    bool maxSpeed = false;  // Speed only
    bool limit = false;     // Power only: the bench supply in current limit
};

// The item drawn in vector shapes inside [min, max] (a square cell; the probe's is two wide).
void DrawItem(ImDrawList& list, Item item, ImVec2 min, ImVec2 max, const ItemLook& look);

}  // namespace bw
