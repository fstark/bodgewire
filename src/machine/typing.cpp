#include "machine/typing.hpp"

#include "netlist/units.hpp"
#include "parts/keyboard.hpp"

namespace bw {

std::expected<Typing, std::string> ParseTyping(std::string_view text) {
    const std::string bad = "bad typing '" + std::string(text) + "' (expected REF:START:TEXT)";
    const std::size_t first = text.find(':');
    const std::size_t second = first == std::string_view::npos ? first : text.find(':', first + 1);
    if (first == 0 || second == std::string_view::npos || second + 1 == text.size()) {
        return std::unexpected(bad);
    }
    const auto start = ParseDuration(text.substr(first + 1, second - first - 1));
    if (!start) {
        return std::unexpected(bad);
    }
    Typing typing{std::string(text.substr(0, first)), *start, {}};
    const std::string_view keys = text.substr(second + 1);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        char32_t key = static_cast<unsigned char>(keys[i]);
        if (keys[i] == '\\') {
            if (i + 1 == keys.size()) {
                return std::unexpected("bad escape '\\' at the end");
            }
            const char escaped = keys[++i];
            if (escaped == 'n') {
                key = U'\r';
            } else if (escaped == 'b') {
                key = U'\b';
            } else if (escaped == '\\') {
                key = U'\\';
            } else {
                return std::unexpected("bad escape '\\" + std::string(1, escaped) + "'");
            }
        }
        const auto code = KeyboardCode(key);
        if (!code) {
            return std::unexpected("cannot type '" + std::string(1, keys[i]) + "' on a keyboard");
        }
        typing.keys.push_back(*code);
    }
    return typing;
}

}  // namespace bw
