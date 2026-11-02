#pragma once

#include <string_view>

#include "machine/crack.hpp"
#include "machine/puzzle.hpp"

namespace bw {

// The one crack of puzzles/<puzzle>.bwp, placed.
Crack PuzzleCrack(std::string_view puzzle);
// A crack placed on the fault-free netlists/<machine>.bwn Board.
Crack BoardCrack(std::string_view machine, const PinRef& a, const PinRef& b, double percent);

// Cached cracks shared by tests.
const Crack& H0Crack();      // test_pattern H0, U1.QA → U13.2B: puzzles/broken_trace.bwp
const Crack& C19Crack();     // counter C19, U5.QD → D1 pad 1: puzzles/dark_led.bwp
const Crack& LedA5Crack();   // chaser LED_A5, R6.2 → D5 pad 2: puzzles/gap.bwp
const Crack& SerCrack();     // chaser SER, U6 pin 8 → U5 pin 1: puzzles/dark_chaser.bwp
const Crack& BtnNCrack();    // dice BTN_N, SW1 pad 1 → R9 pad 2: puzzles/dead_button.bwp
const Crack& N2Crack();      // dice N2, U2 pin 3 → U4 pin 6: puzzles/loaded_dice.bwp
const Crack& R1VccCrack();   // counter VCC, R1.1 → U1.16: R1's rail side cut off
const Crack& Sw1GndCrack();  // dice GND, SW1 pad 2 → J1.2: SW1's rail end cut off

}  // namespace bw
