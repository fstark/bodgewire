#pragma once

#include "machine/puzzle.hpp"

namespace bw {

// Midpoint of the longest straight H0 Trace (U1.QA → U13.2B) of boards/test_pattern; also in
// puzzles/broken_trace.bwp.
inline constexpr Point kH0Crack{137'590'000, 99'170'000};

// Midpoint of the long segment of the only VCC Trace at R1.1 of boards/counter (the stub at the
// pad is too short to crack): R1's rail side cut off.
inline constexpr CrackDecl kR1VccCracks[] = {{1, Layer::Back, {128'310'000, 106'300'000}}};

}  // namespace bw
