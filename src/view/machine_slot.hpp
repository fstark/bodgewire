#pragma once

#include <cstdint>
#include <span>

namespace bw {

// One of the Machine's objects in the shelf's Machine slot (P6o): the TV, the keyboard, the knob
// box. Out (its window shown), in front of every other window but the slot's own, popped out to
// its own desktop window (P6j).
struct SlotObject {
    bool out = false, front = false, popped = false;
};

enum class SlotClick : std::uint8_t { BringOut, ToFront, PutAway };

// A click on the slot: any of them away, all out and in front; all out but some behind another
// window, all to the front; all out and in front, all away. None: BringOut (the slot is inert).
SlotClick ClickSlot(std::span<const SlotObject> objects);
// The second click of a double-click, after the first acted: true, every one popped out; false,
// every one back in (all were popped out already).
bool PopAll(std::span<const SlotObject> objects);
// The slot's width in cells: one per device, at least one (a grayed monitor when none).
int MachineSlotCells(bool tv, bool keyboard, bool knob);

}  // namespace bw
