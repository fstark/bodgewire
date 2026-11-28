#pragma once

#include <string_view>

#include "machine/crack.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"

namespace bw {

// The one crack of the Puzzle <puzzle> (test::PuzzlePath), placed.
Crack PuzzleCrack(std::string_view puzzle);
// A crack placed on the fault-free Board of <machine> (test::BoardPath).
Crack BoardCrack(std::string_view machine, const PinRef& a, const PinRef& b, double percent);

// A Puzzle from its text ("machine led_computer\nfault ..."), loaded against the repository: a
// Fault tested before its Puzzle file exists.
Machine InlinePuzzle(std::string_view text);

// The period the oscillator can `reference` runs at in a built Circuit (a Wrong Clock's, P8k).
Time CanPeriod(const LoadedCircuit& loaded, std::string_view reference);

// Cached cracks shared by tests.
const Crack& H0Crack();  // test_pattern H0, U1.QA → U13.2B:
                         // machines/050-test-pattern/puzzles/01-broken-trace.bwp
const Crack&
C19Crack();  // counter C19, U5.QD → D1 pad 1: machines/010-counter/puzzles/01-dark-led.bwp
const Crack&
LedA5Crack();  // chaser LED_A5, R6.2 → D5 pad 2: machines/020-led-chaser/puzzles/01-gap.bwp
const Crack&
SerCrack();  // chaser SER, U6 pin 8 → U5 pin 1: machines/020-led-chaser/puzzles/03-dark-chaser.bwp
const Crack& VccR5R6Crack();  // chaser VCC, R5 pad 1 → R6 pad 1:
                              // machines/020-led-chaser/puzzles/05-half-life.bwp
const Crack&
BtnNCrack();  // dice BTN_N, SW1 pad 1 → R9 pad 2: machines/030-dice/puzzles/01-dead-button.bwp
const Crack&
N2Crack();  // dice N2, U2 pin 3 → U4 pin 6: machines/030-dice/puzzles/02-loaded-dice.bwp
const Crack& ZipsCrack();           // test_pattern LINE_END, U9 pin 6 → U3 pin 7:
                                    // machines/050-test-pattern/puzzles/08-zips.bwp
const Crack& NineteenCrack();       // stopwatch T0, U6 pin 1 → U6 pin 12:
                                    // machines/040-stopwatch/puzzles/02-nineteen.bwp
const Crack& BrokenBarCrack();      // stopwatch DS1_E, U8 pin 9 → DS1 pad 1:
                                    // machines/040-stopwatch/puzzles/04-broken-bar.bwp
const Crack& NonStopCrack();        // stopwatch RUN, U11 pin 5 → U12 pin 2:
                                    // machines/040-stopwatch/puzzles/05-non-stop.bwp
const Crack& DarkSecondsCrack();    // stopwatch VCC, U5 pin 5 → U6 pin 5:
                                    // machines/040-stopwatch/puzzles/06-dark-seconds.bwp
const Crack& JumpingBallCrack();    // ball VLOAD_N, U24 pin 6 → U21 pin 9:
                                    // machines/060-bouncing-ball/puzzles/04-jumping-ball.bwp
const Crack& DeadKnobCrack();       // knob VCC, J2 pin 3 → U6 pin 14:
                                    // machines/045-knob-meter/puzzles/01-dead-knob.bwp
const Crack& SingleFiguresCrack();  // knob C3, U8 pin 11 → U7 pin 14:
                                    // machines/045-knob-meter/puzzles/03-single-figures.bwp
const Crack& LongPaddleCrack();     // tennis LPAD_CK, U32 pin 1 → U28 pin 14:
                                    // machines/070-tennis/puzzles/04-long-paddle.bwp
const Crack& InvisibleBallCrack();  // tennis SERVING, U43 pin 10 → U38 pin 10:
                                    // machines/070-tennis/puzzles/05-invisible-ball.bwp
const Crack& GibberishCrack();      // message CH5, U14 pin 15 → U15 pin 22:
                                    // machines/080-message/puzzles/03-gibberish.bwp
const Crack& BlockLettersCrack();   // message CLK_N, U5 pin 6 → U16 pin 2:
                                    // machines/080-message/puzzles/06-block-letters.bwp
const Crack& LookalikesCrack();     // typewriter PHI2_N, U17 pin 1 → U18 pin 1:
                                    // machines/090-typewriter/puzzles/01-lookalikes.bwp
const Crack& MIsReturnCrack();      // typewriter CTL, U12 pin 4 → U30 pin 1:
                                    // machines/090-typewriter/puzzles/06-m-is-return.bwp
const Crack& LightsOutCrack();      // led_computer LATCH_CLK, U6 pin 13 → U7 pin 11:
                                    // machines/110-led-computer/puzzles/01-lights-out.bwp
const Crack& R1VccCrack();          // counter VCC, R1.1 → U1.16: R1's rail side cut off
const Crack& Sw1GndCrack();         // dice GND, SW1 pad 2 → J1.2: SW1's rail end cut off
const Crack& HalfSegmentCrack();    // bw1 SEG_A, DS3 pad 7 → DS4 pad 7:
                                    // machines/120-bw1/puzzles/03-half-segment.bwp
const Crack&
DeadDaCrack();  // bw1 PA3, U17 pin 8 → SW18 pad 2: machines/120-bw1/puzzles/05-dead-da.bwp
const Crack&
DeadRowCrack();  // bw1 ROW2, U24 pin 3 → SW20 pad 1: machines/120-bw1/puzzles/06-dead-row.bwp
const Crack& WrongSuspectCrack();  // bw1 DOUT3, U9 pin 12 → U13 pin 9:
                                   // machines/120-bw1/puzzles/14-wrong-suspect.bwp
const Crack&
HalfByteCrack();  // bw1 BUF_N, U14 pin 1 → U13 pin 4: machines/120-bw1/puzzles/15-half-byte.bwp
const Crack& StripedScreenCrack();  // bw2001 CD3, UA2 pin 13 → UB2 pin 14:
                                    // machines/130-bw2001/puzzles/02-striped-screen.bwp
const Crack& RollingTextCrack();    // bw2001 VERT_DRIVE, J7 pad 3 → UD8 pin 11:
                                    // machines/130-bw2001/puzzles/03-rolling-text.bwp
// bw2001 KIN0, J5 pad 11 → UG8 pin 10: machines/130-bw2001/puzzles/07-missing-keys.bwp
const Crack& MissingKeysCrack();
// bw2001 OSC8, X1 pad 8 → UE2 pin 5: machines/130-bw2001/puzzles/16-no-clock.bwp
const Crack& NoClockCrack();
const Crack& BottomHalvesCrack();  // bw80 LC2, IC21 pin 11 → IC8 pin 14:
                                   // machines/140-bw80/puzzles/03-bottom-halves.bwp
const Crack&
EightKeysCrack();  // bw80 KB2, IC10 pin 12 → R15 pad 1: machines/140-bw80/puzzles/08-eight-keys.bwp
const Crack&
NoPrintCrack();  // bw80 A13, J3 pad 6 → IC1 pin 3: machines/140-bw80/puzzles/10-no-print.bwp
const Crack& RollingScreenCrack();    // bw80 KBD_N, IC17 pin 11 → IC11 pin 13:
                                      // machines/140-bw80/puzzles/11-rolling-screen.bwp
const Crack& VerticalStripesCrack();  // bw80 ROM_CS1, IC6 pin 7 → IC2 pin 20:
                                      // machines/140-bw80/puzzles/14-vertical-stripes.bwp

}  // namespace bw
