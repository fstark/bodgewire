#pragma once

#include <cstdint>

#include <imgui.h>

#include "instruments/logic_probe.hpp"

namespace bw {

// Everything on the shelf (P6c; regrouped in P6o), left to right: the way back to the launcher
// (P7a), the controls, the Board's tools, the instruments; at the right end this Machine's brief,
// binder and Machine slot.
enum class Item : std::uint8_t {
    Leave,  // back to the launcher (P7a)
    Power,
    RunSwitch,  // pause, real time, as fast as possible
    Reset,      // every repair undone, asked first
    Pencil,     // the grease pencil (P6h), the first of the Board's tools
    Probe,
    Puller,
    Iron,
    Cutters,
    Wire,
    Analyzer,
    Scope,
    Meter,
    Brief,  // the Puzzle brief, a post-it on the Board
    Manual,
    Machine  // the Machine slot (P6o): its TV, keyboard and knob box
};

// The run switch's three positions (P6o): `‖`, `1×`, `Max`.
enum class RunSwitch : std::uint8_t { Pause, RealTime, Max };

// What the Machine slot draws: each device the Machine has, lit while its object is out.
struct SlotLook {
    bool tv = false, keyboard = false, knob = false;
    bool tvLit = false, keyboardLit = false, knobLit = false;
};

struct ItemLook {
    float alpha = 1.0f;                   // 0.35 grayed
    bool lit = false;                     // equipment out, power on, a tool held
    ProbeLamps lamps{};                   // Probe only
    RunSwitch run = RunSwitch::RealTime;  // RunSwitch only
    bool powered = false;                 // RunSwitch only: `‖` printed grayed while off
    bool running = false;                 // RunSwitch only: at Max, the speed in place of "Max"
    bool behind = false;                  // RunSwitch only: behind real time, the speed shown
    double speed = 0;                     // RunSwitch only: simulated over wall time
    bool limit = false;                   // Power only: the bench supply in current limit
    SlotLook slot{};                      // Machine only
};

// The item drawn in vector shapes inside [min, max]: a square cell, or several side by side (the
// probe's and the run switch's two, the Machine slot's one per device).
void DrawItem(ImDrawList& list, Item item, ImVec2 min, ImVec2 max, const ItemLook& look);

}  // namespace bw
