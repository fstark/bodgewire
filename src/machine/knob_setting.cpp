#include "machine/knob_setting.hpp"

#include <cstddef>

namespace bw {

namespace {

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

std::expected<KnobSetting, std::string> ParseKnob(std::string_view text) {
    const std::string bad = "bad knob '" + std::string(text) + "' (expected REF:N%, 0 to 100)";
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos || colon == 0) {
        return std::unexpected(bad);
    }
    const std::string_view number = text.substr(colon + 1);
    std::size_t i = 0;
    double value = 0;
    for (; i < number.size() && IsDigit(number[i]); ++i) {
        value = value * 10 + (number[i] - '0');
    }
    if (i == 0) {
        return std::unexpected(bad);
    }
    if (i < number.size() && number[i] == '.') {
        ++i;
        double scale = 0.1;
        const std::size_t start = i;
        for (; i < number.size() && IsDigit(number[i]); ++i) {
            value += (number[i] - '0') * scale;
            scale /= 10;
        }
        if (i == start) {
            return std::unexpected(bad);
        }
    }
    if (number.substr(i) != "%" || value > 100) {
        return std::unexpected(bad);
    }
    return KnobSetting{std::string(text.substr(0, colon)), value / 100};
}

}  // namespace bw
