#include "view/meter_reading.hpp"

#include <cmath>
#include <format>

#include "sim/check.hpp"

namespace bw {

MeterReading VoltsReading(std::optional<double> volts) {
    if (!volts) {
        return {"---"};
    }
    const double v = std::abs(*volts);
    return {v < 9.995 ? std::format("{:.2f}", v) : std::format("{:.1f}", v)};
}

MeterReading ContinuityReading(std::optional<bool> connected) {
    const bool beep = connected.value_or(false);
    return {beep ? "0.0" : "OL", true, beep};
}

std::uint8_t Segments(char c) {
    constexpr std::uint8_t a = 1, b = 2, cc = 4, d = 8, e = 16, f = 32, g = 64;
    switch (c) {
    case '0':
    case 'O':
        return a | b | cc | d | e | f;
    case '1':
        return b | cc;
    case '2':
        return a | b | g | e | d;
    case '3':
        return a | b | g | cc | d;
    case '4':
        return f | g | b | cc;
    case '5':
        return a | f | g | cc | d;
    case '6':
        return a | f | g | e | cc | d;
    case '7':
        return a | b | cc;
    case '8':
        return a | b | cc | d | e | f | g;
    case '9':
        return a | b | cc | d | f | g;
    case '-':
        return g;
    case 'L':
        return d | e | f;
    case ' ':
        return 0;
    default:
        Check(false, "meter reading: no segments for that character");
        return 0;
    }
}

}  // namespace bw
