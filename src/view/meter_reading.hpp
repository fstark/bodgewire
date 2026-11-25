#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace bw {

inline constexpr std::size_t kMeterCells = 3;

// What the Multimeter's LCD shows (P8g): kMeterCells seven-segment cells and the mode's sign.
struct MeterReading {
    std::string digits;       // the cells' characters, right-aligned; a '.' lights the decimal
                              // point of the cell before it
    bool continuity = false;  // the •))) sign; else V
    bool beep = false;        // connected: the backlight green
    bool operator==(const MeterReading&) const = default;
};

// "2.43" (two decimals under 9.995 V), "12.0" (one from there), "---" for nullopt.
MeterReading VoltsReading(std::optional<double> volts);
// "0.0" lit green when connected, "OL" otherwise (nullopt: off copper, or no red Lead).
MeterReading ContinuityReading(std::optional<bool> connected);
// Seven-segment bits, a = 1 (top) clockwise to f = 32, g = 64 (middle), for '0'-'9', '-', 'O',
// 'L' and ' '; Check()s anything else.
std::uint8_t Segments(char c);

}  // namespace bw
