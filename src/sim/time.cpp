#include "sim/time.hpp"

#include <array>
#include <cmath>

#include "text/text.hpp"

namespace bw {

namespace {

constexpr double kMaxPicos = 1e18;  // keeps llround in range
constexpr double kMaxDuration = kMaxPicos;
constexpr double kMaxPeriod = kMaxPicos;

struct DurationUnit {
    std::string_view name;
    double picos;
};

constexpr std::array<DurationUnit, 5> kDurationUnits{
    {{"ps", 1}, {"ns", 1e3}, {"us", 1e6}, {"ms", 1e9}, {"s", 1e12}}};

}  // namespace

std::expected<Time, std::string> ParseDuration(std::string_view text) {
    const auto bad = [&] {
        return std::unexpected("bad duration '" + std::string(text) +
                               "': expected a number then ps, ns, us, ms or s, e.g. 10ms");
    };
    const auto number = ParseDecimal(text);
    if (!number) {
        return bad();
    }
    const double value = number->value;
    const std::string_view unit = text.substr(number->length);
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

std::expected<Time, std::string> ParseFrequencyPeriod(std::string_view text) {
    const auto bad = [&] {
        return std::unexpected("bad frequency '" + std::string(text) +
                               "': expected a number then Hz, kHz or MHz, e.g. 14.31818MHz");
    };
    const auto number = ParseDecimal(text);
    if (!number) {
        return bad();
    }
    const double value = number->value;
    const std::string_view unit = text.substr(number->length);
    double multiplier = 0;
    if (unit == "Hz") {
        multiplier = 1;
    } else if (unit == "kHz") {
        multiplier = 1e3;
    } else if (unit == "MHz") {
        multiplier = 1e6;
    } else {
        return bad();
    }
    if (value <= 0) {
        return std::unexpected("frequency must be > 0");
    }
    const double period = 1e12 / (value * multiplier);
    if (period > kMaxPeriod) {
        return std::unexpected("frequency too low");
    }
    const Time rounded = std::llround(period);
    if (rounded < 2) {
        return std::unexpected("frequency too high");
    }
    return rounded;
}

}  // namespace bw
