#include "netlist/units.hpp"

#include "parts/oscillator.hpp"
#include "parts/paddle.hpp"

namespace bw {

std::expected<Time, std::string> ParsePeriodFromFrequency(std::string_view text) {
    return PeriodFromFrequency(text);
}

std::expected<Time, std::string> ParseDuration(std::string_view text) {
    return DurationFromText(text);
}

}  // namespace bw
