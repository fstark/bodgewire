#include "view/contents_defaults.hpp"

#include <charconv>
#include <format>
#include <vector>

#include "text/text.hpp"

namespace bw {

namespace {

// "$1FF" or "511".
std::optional<std::uint32_t> Number(std::string_view text) {
    const bool hex = text.starts_with('$');
    if (hex) {
        text.remove_prefix(1);
    }
    std::uint32_t value = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value, hex ? 16 : 10);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

}  // namespace

std::expected<ContentsDefaults, std::string> ParseContentsDefaults(std::string_view text,
                                                                   std::string_view file) {
    ContentsDefaults defaults;
    bool charsetSeen = false;
    std::size_t number = 0;
    while (!text.empty()) {
        ++number;
        const std::size_t eol = text.find('\n');
        std::string_view line = text.substr(0, eol);
        text = eol == std::string_view::npos ? std::string_view{} : text.substr(eol + 1);
        if (const std::size_t hash = line.find('#'); hash != std::string_view::npos) {
            line = line.substr(0, hash);
        }
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        const auto words = SplitWords(line);
        if (words.empty()) {
            continue;
        }
        const auto fail = [&](std::string_view what) {
            return std::unexpected(std::format("{} line {}: {}", file, number, what));
        };
        if (words[0] == "cpu") {
            if (words.size() != 2 || (words[1] != "6502" && words[1] != "z80")) {
                return fail("cpu takes 6502 or z80");
            }
            if (defaults.cpu) {
                return fail("a second cpu");
            }
            defaults.cpu = words[1] == "6502" ? Cpu::Mos6502 : Cpu::Z80;
        } else if (words[0] == "charset") {
            if (words.size() != 2 ||
                (words[1] != "ascii" && words[1] != "petscii" && words[1] != "zx80")) {
                return fail("charset takes ascii, petscii or zx80");
            }
            if (charsetSeen) {
                return fail("a second charset");
            }
            charsetSeen = true;
            defaults.charset = words[1] == "ascii"     ? CharacterSet::Ascii
                               : words[1] == "petscii" ? CharacterSet::Petscii
                                                       : CharacterSet::Zx80;
        } else if (words[0] == "chip") {
            if (words.size() < 2) {
                return fail("chip needs a reference");
            }
            if (defaults.chips.contains(words[1])) {
                return fail(std::format("chip {} twice", words[1]));
            }
            if (words.size() % 2 != 0) {
                return fail("chip REF [address N] [glyphs N]");
            }
            ChipDefaults chip;
            for (std::size_t i = 2; i < words.size(); i += 2) {
                const auto value = Number(words[i + 1]);
                if (!value) {
                    return fail(std::format("bad number '{}'", words[i + 1]));
                }
                if (words[i] == "address" && !chip.address) {
                    if (*value > 0xFFFF) {
                        return fail("address over $FFFF");
                    }
                    chip.address = static_cast<std::uint16_t>(*value);
                } else if (words[i] == "glyphs" && !chip.glyphs) {
                    chip.glyphs = *value;
                } else {
                    return fail(std::format("unknown or repeated '{}'", words[i]));
                }
            }
            defaults.chips.emplace(std::string(words[1]), chip);
        } else {
            return fail(std::format("unknown keyword '{}'", words[0]));
        }
    }
    return defaults;
}

}  // namespace bw
