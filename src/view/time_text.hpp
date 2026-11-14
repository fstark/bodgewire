#pragma once

#include <string>

#include "sim/time.hpp"

namespace bw {

// Three significant digits, trailing zeros dropped; `sign`: "+" before positive values.
std::string FormatTime(Time ps, bool sign);
// 1 / period, three significant digits, trailing zeros dropped: "588 kHz", "1.67 MHz".
std::string FormatFrequency(Time period);

}  // namespace bw
