#pragma once

#include "machine/puzzle.hpp"

namespace bw {

// Midpoint of the longest straight H0 Trace (U1.QA → U13.2B) of boards/test_pattern; also in
// puzzles/broken_trace.bwp.
inline constexpr Point kH0Crack{137'590'000, 99'170'000};

// Midpoints of both VCC Traces at R1.1 of boards/counter: R1's rail side cut off.
inline constexpr CrackDecl kR1VccCracks[] = {{1, Layer::Front, {127'810'000, 71'690'000}},
                                             {2, Layer::Back, {126'500'000, 76'110'000}}};

}  // namespace bw
