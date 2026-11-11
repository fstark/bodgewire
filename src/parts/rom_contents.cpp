#include "parts/rom_contents.hpp"

#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>

namespace bw {

namespace {

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r'; }

std::optional<unsigned> ParseNumber(std::string_view text, int base) {
    unsigned value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

class ContentsParser {
public:
    ContentsParser(std::string_view file, std::size_t size, int width)
        : file_(file), size_(size), mask_(static_cast<unsigned>((1u << width) - 1)), width_(width),
          words_(size, static_cast<std::uint8_t>(mask_)), lines_(size, 0) {}

    std::expected<std::vector<std::uint8_t>, std::string> parse(std::string_view text) {
        while (!text.empty() && error_.empty()) {
            ++line_;
            const std::size_t newline = text.find('\n');
            const std::string_view current = text.substr(0, newline);
            text =
                newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
            parseLine(current);
        }
        if (!error_.empty()) {
            return std::unexpected(error_);
        }
        return std::move(words_);
    }

private:
    void fail(std::string what) {
        if (error_.empty()) {
            error_ = std::format("{} line {}: {}", file_, line_, what);
        }
    }

    void put(unsigned value) {
        if (address_ >= size_) {
            return fail(
                std::format("address 0x{:03X} is past the end (size 0x{:03X})", address_, size_));
        }
        if (lines_[address_] != 0) {
            return fail(std::format("address 0x{:03X} already written at line {}", address_,
                                    lines_[address_]));
        }
        words_[address_] = static_cast<std::uint8_t>(value);
        lines_[address_] = line_;
        ++address_;
    }

    void parseLine(std::string_view text) {
        std::size_t i = 0;
        while (i < text.size() && error_.empty()) {
            if (IsSpace(text[i])) {
                ++i;
                continue;
            }
            if (text[i] == ';') {
                return;
            }
            if (text[i] == '"') {
                i = parseString(text, i + 1);
                continue;
            }
            std::size_t end = i;
            while (end < text.size() && !IsSpace(text[end]) && text[end] != ';') {
                ++end;
            }
            parseToken(text.substr(i, end - i));
            i = end;
        }
    }

    // Returns the index after the closing quote.
    std::size_t parseString(std::string_view text, std::size_t i) {
        while (i < text.size() && error_.empty()) {
            char c = text[i];
            if (c == '"') {
                return i + 1;
            }
            if (c == '\\') {
                if (i + 1 >= text.size()) {
                    break;
                }
                c = text[i + 1];
                if (c != '"' && c != '\\') {
                    fail(std::format("bad escape '\\{}'", c));
                    return text.size();
                }
                ++i;
            }
            const auto code = static_cast<unsigned char>(c);
            if (code < 0x20 || code > 0x7E) {
                fail("bad character in string");
                return text.size();
            }
            if (code > mask_) {
                fail(std::format("word 0x{:02X} is wider than {} bits", code, width_));
                return text.size();
            }
            put(code);
            ++i;
        }
        fail("unterminated string");
        return text.size();
    }

    void parseToken(std::string_view token) {
        if (token.front() == '@') {
            const auto value = token.size() <= 5 ? ParseNumber(token.substr(1), 16) : std::nullopt;
            if (!value) {
                return fail(std::format("bad address '{}'", token));
            }
            address_ = *value;
            return;
        }
        if (token.front() == '.' || token.front() == '#') {
            unsigned value = 0;
            for (const char c : token) {
                if (c != '.' && c != '#') {
                    return fail(std::format("unknown token '{}'", token));
                }
                value = value * 2 + (c == '#' ? 1 : 0);
            }
            if (token.size() > static_cast<std::size_t>(width_)) {
                return fail(std::format("dot row '{}' is wider than {} bits", token, width_));
            }
            return put(value);
        }
        const std::size_t star = token.find('*');
        const std::string_view hex = token.substr(0, star);
        const auto value = hex.size() <= 2 ? ParseNumber(hex, 16) : std::nullopt;
        std::optional<unsigned> count = 1;
        if (star != std::string_view::npos) {
            count = ParseNumber(token.substr(star + 1), 10);
        }
        if (!value || !count || *count == 0) {
            return fail(std::format("unknown token '{}'", token));
        }
        if (*value > mask_) {
            return fail(std::format("word 0x{:02X} is wider than {} bits", *value, width_));
        }
        for (unsigned n = 0; n < *count && error_.empty(); ++n) {
            put(*value);
        }
    }

    std::string_view file_;
    std::size_t size_;
    unsigned mask_;
    int width_;
    std::vector<std::uint8_t> words_;
    std::vector<std::uint32_t> lines_;  // line that wrote each word, 0 if none
    std::size_t address_ = 0;
    std::uint32_t line_ = 0;
    std::string error_;
};

}  // namespace

std::expected<std::vector<std::uint8_t>, std::string>
ParseRomContents(std::string_view text, std::string_view file, std::size_t size, int width) {
    return ContentsParser(file, size, width).parse(text);
}

std::expected<std::vector<std::uint8_t>, std::string>
ReadRomContents(const std::filesystem::path& path, std::size_t size, int width) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::unexpected("cannot read '" + path.string() + "'");
    }
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    return ParseRomContents(text, path.filename().string(), size, width);
}

}  // namespace bw
