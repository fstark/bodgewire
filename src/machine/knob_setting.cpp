#include "machine/knob_setting.hpp"

#include <cstddef>

#include "text/text.hpp"

namespace bw {

std::expected<KnobSetting, std::string> ParseKnob(std::string_view text) {
    const std::string bad =
        "bad knob '" + std::string(text) + "' (expected REF:N% or REF:N%:TIME, 0 to 100)";
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos || colon == 0) {
        return std::unexpected(bad);
    }
    const std::size_t second = text.find(':', colon + 1);
    const bool timed = second != std::string_view::npos;
    const auto percent =
        ParsePercent(text.substr(colon + 1, timed ? second - colon - 1 : std::string_view::npos));
    if (!percent) {
        return std::unexpected(bad);
    }
    KnobSetting knob{std::string(text.substr(0, colon)), *percent / 100};
    if (timed) {
        const auto at = ParseDuration(text.substr(second + 1));
        if (!at) {
            return std::unexpected(bad);
        }
        knob.at = *at;
    }
    return knob;
}

}  // namespace bw
