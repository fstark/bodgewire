#pragma once

#include <string>

#include "sim/time.hpp"

namespace bw {

// Three significant digits, trailing zeros dropped; `sign`: "+" before positive values.
std::string FormatTime(Time ps, bool sign);

}  // namespace bw
