#pragma once

#include <string_view>

#include "machine/crack.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"

namespace bw {

// The one crack of puzzles/<puzzle>.bwp, placed.
Crack PuzzleCrack(std::string_view puzzle);
// A crack placed on the fault-free netlists/<machine>.bwn Board.
Crack BoardCrack(std::string_view machine, const PinRef& a, const PinRef& b, double percent);

// A Puzzle from its text ("machine led_computer\nfault ..."), loaded against the repository: a
// Fault tested before its Puzzle file exists.
Machine InlinePuzzle(std::string_view text);

// Cached cracks shared by tests.
const Crack& H0Crack();             // test_pattern H0, U1.QA → U13.2B: puzzles/broken_trace.bwp
const Crack& C19Crack();            // counter C19, U5.QD → D1 pad 1: puzzles/dark_led.bwp
const Crack& LedA5Crack();          // chaser LED_A5, R6.2 → D5 pad 2: puzzles/gap.bwp
const Crack& SerCrack();            // chaser SER, U6 pin 8 → U5 pin 1: puzzles/dark_chaser.bwp
const Crack& BtnNCrack();           // dice BTN_N, SW1 pad 1 → R9 pad 2: puzzles/dead_button.bwp
const Crack& N2Crack();             // dice N2, U2 pin 3 → U4 pin 6: puzzles/loaded_dice.bwp
const Crack& ZipsCrack();           // test_pattern LINE_END, U9 pin 6 → U3 pin 7: puzzles/zips.bwp
const Crack& NineteenCrack();       // stopwatch T0, U6 pin 1 → U6 pin 12: puzzles/nineteen.bwp
const Crack& BrokenBarCrack();      // stopwatch DS1_E, U8 pin 9 → DS1 pad 1: puzzles/broken_bar.bwp
const Crack& NonStopCrack();        // stopwatch RUN, U11 pin 5 → U12 pin 2: puzzles/non_stop.bwp
const Crack& DarkSecondsCrack();    // stopwatch VCC, U5 pin 5 → U6 pin 5: puzzles/dark_seconds.bwp
const Crack& JumpingBallCrack();    // ball VLOAD_N, U24 pin 6 → U21 pin 9: puzzles/jumping_ball.bwp
const Crack& DeadKnobCrack();       // knob VCC, J2 pin 3 → U6 pin 14: puzzles/dead_knob.bwp
const Crack& SingleFiguresCrack();  // knob C3, U8 pin 11 → U7 pin 14: puzzles/single_figures.bwp
const Crack& LongPaddleCrack();  // tennis LPAD_CK, U32 pin 1 → U28 pin 14: puzzles/long_paddle.bwp
const Crack&
InvisibleBallCrack();  // tennis SERVING, U43 pin 10 → U38 pin 10: puzzles/invisible_ball.bwp
const Crack& GibberishCrack();     // message CH5, U14 pin 15 → U15 pin 22: puzzles/gibberish.bwp
const Crack& BlockLettersCrack();  // message CLK_N, U5 pin 6 → U16 pin 2: puzzles/block_letters.bwp
const Crack& LookalikesCrack();  // typewriter PHI2_N, U17 pin 1 → U18 pin 1: puzzles/lookalikes.bwp
const Crack& MIsReturnCrack();   // typewriter CTL, U12 pin 4 → U30 pin 1: puzzles/m_is_return.bwp
const Crack& FlatlineCrack();    // heartbeat PHI0, X1 OUT → U1 pin 37: puzzles/flatline.bwp
const Crack& FifteenCrack();     // heartbeat A14, U1 pin 24 → U3 pin 6: puzzles/fifteen.bwp
const Crack&
LightsOutCrack();           // led_computer LATCH_CLK, U6 pin 13 → U7 pin 11: puzzles/lights_out.bwp
const Crack& R1VccCrack();  // counter VCC, R1.1 → U1.16: R1's rail side cut off
const Crack& Sw1GndCrack();        // dice GND, SW1 pad 2 → J1.2: SW1's rail end cut off
const Crack& HalfSegmentCrack();   // bw1 SEG_A, DS3 pad 7 → DS4 pad 7: puzzles/half_segment.bwp
const Crack& DeadDaCrack();        // bw1 PA3, U17 pin 8 → SW18 pad 2: puzzles/dead_da.bwp
const Crack& DeadRowCrack();       // bw1 ROW2, U24 pin 3 → SW20 pad 1: puzzles/dead_row.bwp
const Crack& WrongSuspectCrack();  // bw1 DOUT3, U9 pin 12 → U13 pin 9: puzzles/wrong_suspect.bwp
const Crack& HalfByteCrack();      // bw1 BUF_N, U14 pin 1 → U13 pin 4: puzzles/half_byte.bwp

}  // namespace bw
