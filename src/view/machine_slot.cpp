#include "view/machine_slot.hpp"

#include <algorithm>

namespace bw {

SlotClick ClickSlot(std::span<const SlotObject> objects) {
    if (objects.empty() ||
        std::ranges::any_of(objects, [](const SlotObject& o) { return !o.out; })) {
        return SlotClick::BringOut;
    }
    if (std::ranges::any_of(objects, [](const SlotObject& o) { return !o.front; })) {
        return SlotClick::ToFront;
    }
    return SlotClick::PutAway;
}

bool PopAll(std::span<const SlotObject> objects) {
    return !std::ranges::all_of(objects, [](const SlotObject& o) { return o.popped; });
}

int MachineSlotCells(bool tv, bool keyboard, bool knob) {
    return std::max(1, static_cast<int>(tv) + static_cast<int>(keyboard) + static_cast<int>(knob));
}

}  // namespace bw
