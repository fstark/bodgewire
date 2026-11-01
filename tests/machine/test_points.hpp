#pragma once

#include "machine/puzzle.hpp"

namespace bw {

// Midpoint of the longest straight H0 Trace (U1.QA → U13.2B) of boards/test_pattern; also in
// puzzles/broken_trace.bwp.
inline constexpr Point kH0Crack{137'590'000, 99'170'000};

// Midpoint of the long segment of the only VCC Trace at R1.1 of boards/counter (the stub at the
// pad is too short to crack): R1's rail side cut off.
inline constexpr CrackDecl kR1VccCracks[] = {{1, Layer::Back, {128'310'000, 106'300'000}}};

// Midpoint of the straight C19 Trace (U5.QD → D1.K) of boards/counter; also in
// puzzles/dark_led.bwp.
inline constexpr Point kC19Crack{192'520'000, 122'000'000};

// Midpoint of the straight LED_A5 Trace (R6.2 → D5.A) of boards/chaser; also in puzzles/gap.bwp.
inline constexpr Point kLedA5Crack{73'460'000, 101'050'000};

// Midpoint of the long straight SER Trace (U6.Y → U5.A) of boards/chaser; also in
// puzzles/dark_chaser.bwp.
inline constexpr Point kSerCrack{70'170'000, 73'690'000};

}  // namespace bw
