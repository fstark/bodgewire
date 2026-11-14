#include "view/time_text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <span>
#include <utility>

namespace bw {

namespace {

using Unit = std::pair<double, const char*>;

// `magnitude` > 0 in the largest unit it reaches (else the last), three significant digits,
// trailing zeros dropped.
std::string Scaled(double magnitude, std::span<const Unit> units) {
    Unit unit = units.back();
    for (const Unit& u : units) {
        if (magnitude >= u.first) {
            unit = u;
            break;
        }
    }
    const double value = magnitude / unit.first;
    std::string digits = std::format("{:.{}f}", value, value >= 100 ? 0 : value >= 10 ? 1 : 2);
    if (digits.find('.') != std::string::npos) {
        digits.erase(digits.find_last_not_of('0') + 1);
        if (digits.back() == '.') {
            digits.pop_back();
        }
    }
    return std::format("{} {}", digits, unit.second);
}

}  // namespace

std::string FormatTime(Time ps, bool sign) {
    if (ps == 0) {
        return "0";
    }
    static constexpr std::array<Unit, 5> kUnits{
        {{1e12, "s"}, {1e9, "ms"}, {1e6, "µs"}, {1e3, "ns"}, {1, "ps"}}};
    return std::format("{}{}", ps < 0 ? "-" : (sign ? "+" : ""),
                       Scaled(std::abs(static_cast<double>(ps)), kUnits));
}

std::string FormatTimeStep(Time ps, Time step, bool sign) {
    if (ps == 0) {
        return "0";
    }
    static constexpr std::array<std::pair<Time, const char*>, 5> kUnits{{{kPicosPerSecond, "s"},
                                                                         {1'000'000'000, "ms"},
                                                                         {1'000'000, "\u00b5s"},
                                                                         {1'000, "ns"},
                                                                         {1, "ps"}}};
    const Time magnitude = ps < 0 ? -ps : ps;
    auto unit = kUnits.back();
    for (const auto& u : kUnits) {
        if (magnitude >= u.first) {
            unit = u;
            break;
        }
    }
    int decimals = 0;
    for (Time scaled = std::max<Time>(1, step); scaled % unit.first != 0 && decimals < 12;
         scaled *= 10) {
        ++decimals;
    }
    std::string digits = std::format(
        "{:.{}f}", static_cast<double>(magnitude) / static_cast<double>(unit.first), decimals);
    if (digits.find('.') != std::string::npos) {
        digits.erase(digits.find_last_not_of('0') + 1);
        if (digits.back() == '.') {
            digits.pop_back();
        }
    }
    return std::format("{}{} {}", ps < 0 ? "-" : (sign ? "+" : ""), digits, unit.second);
}

std::string FormatFrequency(Time period) {
    static constexpr std::array<Unit, 4> kUnits{
        {{1e9, "GHz"}, {1e6, "MHz"}, {1e3, "kHz"}, {1, "Hz"}}};
    return Scaled(static_cast<double>(kPicosPerSecond) / static_cast<double>(period), kUnits);
}

}  // namespace bw
