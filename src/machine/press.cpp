#include "machine/press.hpp"

#include "netlist/units.hpp"

namespace bw {

std::expected<ButtonPress, std::string> ParsePress(std::string_view text) {
    const std::string bad = "bad press '" + std::string(text) + "' (expected REF:DOWN:UP)";
    const std::size_t first = text.find(':');
    const std::size_t second = first == std::string_view::npos ? first : text.find(':', first + 1);
    if (first == 0 || second == std::string_view::npos ||
        text.find(':', second + 1) != std::string_view::npos) {
        return std::unexpected(bad);
    }
    const auto down = ParseDuration(text.substr(first + 1, second - first - 1));
    const auto up = ParseDuration(text.substr(second + 1));
    if (!down || !up || *up <= *down) {
        return std::unexpected(bad);
    }
    return ButtonPress{std::string(text.substr(0, first)), *down, *up};
}

}  // namespace bw
