#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace bw {

inline constexpr std::size_t kMeterCells = 4;

// What the Multimeter's LCD shows (P8g): kMeterCells seven-segment cells and a sign.
struct MeterReading {
    std::string digits;  // the cells' characters, right-aligned; a '.' lights the decimal point
                         // of the cell before it
    bool volts = false;  // the V sign
    bool beep = false;   // connected: "bEEP", the •))) sign, the backlight green
    bool operator==(const MeterReading&) const = default;
};

// "2.43" (two decimals under 9.995 V), "12.0" (one from there), "----" for nullopt; V.
MeterReading VoltsReading(std::optional<double> volts);
// "bEEP" with its sign when connected, "----" otherwise (nullopt: off copper, or no red Lead).
// The user, 2026-11-25: `0.0` against `OL` was too alike to read at a glance.
MeterReading ContinuityReading(std::optional<bool> connected);
// Seven-segment bits, a = 1 (top) clockwise to f = 32, g = 64 (middle), for '0'-'9', '-', 'b',
// 'E', 'P' and ' '; Check()s anything else.
std::uint8_t Segments(char c);

}  // namespace bw
