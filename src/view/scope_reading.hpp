#pragma once

#include <optional>
#include <span>
#include <string>

#include "sim/recorder.hpp"

namespace bw {

// A band's reading on the Oscilloscope's screen (P8h): what one Channel's Samples show between
// `from` and `to`, ps since power-on (the Sweep's start, and the screen's right edge: the Sweep's
// end less the divisions under the label plates).
//   High: value One and not floating. A rising edge: a Sample high after one not high (Zero, X,
//   or floating whatever its value: a float driven high counts), the first Sample excluded (it
//   is the value at the start).
//   At least two rising edges: "1 MHz · 50 %": the frequency of their mean period (the first
//   rise to the last over their count less one), the share of that time spent high, to the
//   percent.
//   Else the first whole pulse (a level held from one edge to the next): "high 1 µs", "low 4 µs".
//   Else its first change, from `trigger` (ps since power-on): "rises +64 µs", "falls -2 µs",
//   "floats +1 µs", "unknown +1 µs".
//   Else (no change) its level: "high", "low", "floating 1.4 V", "unknown".
//   A rail (`rail`: its volts): "5.0 V". No Samples: "".
// `samples`: one Channel of a Sweep, the value at its start then each change, in time order.
std::string BandReading(std::span<const Sample> samples, Time from, Time to, Time trigger,
                        std::optional<double> rail);

}  // namespace bw
