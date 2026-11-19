#include "view/machine_slot.hpp"

#include <array>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("machine slot: a click brings out, brings to the front, then puts away", "[view]") {
    // The TV and the keyboard (the Typewriter, the BW-2001, the BW-80).
    const std::array<SlotObject, 2> oneAway{{{true, true, false}, {false, false, false}}};
    CHECK(ClickSlot(oneAway) == SlotClick::BringOut);
    const std::array<SlotObject, 2> oneBehind{{{true, true, false}, {true, false, false}}};
    CHECK(ClickSlot(oneBehind) == SlotClick::ToFront);
    const std::array<SlotObject, 2> allFront{{{true, true, false}, {true, true, true}}};
    CHECK(ClickSlot(allFront) == SlotClick::PutAway);
}

TEST_CASE("machine slot: one object goes as a group of one", "[view]") {
    const std::array<SlotObject, 1> away{{{false, false, false}}};
    CHECK(ClickSlot(away) == SlotClick::BringOut);
    const std::array<SlotObject, 1> behind{{{true, false, false}}};
    CHECK(ClickSlot(behind) == SlotClick::ToFront);
    const std::array<SlotObject, 1> front{{{true, true, false}}};
    CHECK(ClickSlot(front) == SlotClick::PutAway);
}

TEST_CASE("machine slot: a double-click pops all out, or all back in", "[view]") {
    const std::array<SlotObject, 2> none{{{true, true, false}, {true, true, false}}};
    CHECK(PopAll(none));
    const std::array<SlotObject, 2> one{{{true, true, true}, {true, true, false}}};
    CHECK(PopAll(one));
    const std::array<SlotObject, 2> both{{{true, true, true}, {true, true, true}}};
    CHECK_FALSE(PopAll(both));
}

TEST_CASE("machine slot: one cell per device, at least one", "[view]") {
    CHECK(MachineSlotCells(false, false, false) == 1);  // the Counter: a grayed monitor
    CHECK(MachineSlotCells(true, false, false) == 1);   // the Test Pattern
    CHECK(MachineSlotCells(false, false, true) == 1);   // the Knob Meter
    CHECK(MachineSlotCells(true, false, true) == 2);    // Tennis
    CHECK(MachineSlotCells(true, true, false) == 2);    // the Typewriter
}
