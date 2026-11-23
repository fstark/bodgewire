#include "machine/knob_setting.hpp"

#include <cstddef>

#include "text/text.hpp"

namespace bw {

std::expected<KnobSetting, std::string> ParseKnob(std::string_view text) {
    const std::string bad = "bad knob '" + std::string(text) + "' (expected REF:N%, 0 to 100)";
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos || colon == 0) {
        return std::unexpected(bad);
    }
    const auto percent = ParsePercent(text.substr(colon + 1));
    if (!percent) {
        return std::unexpected(bad);
    }
    return KnobSetting{std::string(text.substr(0, colon)), *percent / 100};
}

}  // namespace bw
