#include "netlist/units.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

#include "parts/oscillator.hpp"

namespace bw {

namespace {

constexpr double kMaxDuration = 1e18;  // ps; keeps llround in range

struct DurationUnit {
    std::string_view name;
    double picos;
};

constexpr std::array<DurationUnit, 5> kDurationUnits{
    {{"ps", 1}, {"ns", 1e3}, {"us", 1e6}, {"ms", 1e9}, {"s", 1e12}}};

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

std::expected<Time, std::string> ParsePeriodFromFrequency(std::string_view text) {
    return PeriodFromFrequency(text);
}

std::expected<Time, std::string> ParseDuration(std::string_view text) {
    const auto bad = [&] {
        return std::unexpected("bad duration '" + std::string(text) +
                               "': expected a number then ps, ns, us, ms or s, e.g. 10ms");
    };
    std::size_t i = 0;
    double value = 0;
    for (; i < text.size() && IsDigit(text[i]); ++i) {
        value = value * 10 + (text[i] - '0');
    }
    if (i == 0) {
        return bad();
    }
    if (i < text.size() && text[i] == '.') {
        ++i;
        double scale = 0.1;
        const std::size_t start = i;
        for (; i < text.size() && IsDigit(text[i]); ++i) {
            value += (text[i] - '0') * scale;
            scale /= 10;
        }
        if (i == start) {
            return bad();
        }
    }
    const std::string_view unit = text.substr(i);
    for (const DurationUnit& candidate : kDurationUnits) {
        if (candidate.name != unit) {
            continue;
        }
        const double picos = value * candidate.picos;
        if (picos > kMaxDuration) {
            return std::unexpected("duration too long");
        }
        const Time rounded = std::llround(picos);
        if (rounded < 1) {
            return std::unexpected("duration must be >= 1 ps");
        }
        return rounded;
    }
    return bad();
}

}  // namespace bw
