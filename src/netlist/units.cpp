#include "netlist/units.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "parts/oscillator.hpp"
#include "parts/paddle.hpp"

namespace bw {

std::expected<Time, std::string> ParsePeriodFromFrequency(std::string_view text) {
    return PeriodFromFrequency(text);
}

std::expected<Time, std::string> ParseDuration(std::string_view text) {
    return DurationFromText(text);
}

std::expected<std::uint32_t, std::string> ParseOhms(std::string_view text) {
    const auto bad = [&] {
        return std::unexpected("bad resistance '" + std::string(text) +
                               "': expected e.g. 470, 1k, 4k7 or 1M");
    };
    const auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    std::size_t i = 0;
    while (i < text.size() && isDigit(text[i])) {
        ++i;
    }
    if (i == 0 || i > 7) {
        return bad();
    }
    double whole = std::stod(std::string(text.substr(0, i)));
    double multiplier = 1;
    if (i < text.size()) {
        switch (text[i]) {
        case 'R':
            break;
        case 'k':
        case 'K':
            multiplier = 1e3;
            break;
        case 'M':
            multiplier = 1e6;
            break;
        default:
            return bad();
        }
        const std::string_view fraction = text.substr(i + 1);
        if (fraction.size() > 3 || !std::ranges::all_of(fraction, isDigit)) {
            return bad();
        }
        if (!fraction.empty()) {
            whole += std::stod(std::string(fraction)) /
                     std::pow(10.0, static_cast<double>(fraction.size()));
        }
    }
    const double ohms = std::round(whole * multiplier);
    if (ohms < 1 || ohms > 4e9) {
        return bad();
    }
    return static_cast<std::uint32_t>(ohms);
}

}  // namespace bw
