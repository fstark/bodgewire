#include "ui/time_text.hpp"

#include <array>
#include <cmath>
#include <format>
#include <utility>

namespace bw {

std::string FormatTime(Time ps, bool sign) {
    if (ps == 0) {
        return "0";
    }
    const double magnitude = std::abs(static_cast<double>(ps));
    static constexpr std::array<std::pair<double, const char*>, 5> kUnits{
        {{1e12, "s"}, {1e9, "ms"}, {1e6, "\u00b5s"}, {1e3, "ns"}, {1, "ps"}}};
    auto unit = kUnits.back();
    for (const auto& u : kUnits) {
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
    return std::format("{}{} {}", ps < 0 ? "-" : (sign ? "+" : ""), digits, unit.second);
}

}  // namespace bw
