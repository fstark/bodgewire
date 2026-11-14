#pragma once

#include <string>

#include "sim/time.hpp"

namespace bw {

// Three significant digits, trailing zeros dropped; `sign`: "+" before positive values.
std::string FormatTime(Time ps, bool sign);
// As many decimals as `step` needs in the unit `ps` is shown in, so multiples of `step` never
// print alike: 100.5 µs at a 500 ns step, 98.45 µs at 50 ns; trailing zeros dropped.
std::string FormatTimeStep(Time ps, Time step, bool sign);
// 1 / period, three significant digits, trailing zeros dropped: "588 kHz", "1.67 MHz".
std::string FormatFrequency(Time period);

}  // namespace bw
