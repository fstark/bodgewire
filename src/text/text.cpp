#include "text/text.hpp"

#include <fstream>
#include <iterator>

namespace bw {

std::optional<std::string> ReadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::string_view> SplitWords(std::string_view line) {
    std::vector<std::string_view> words;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && IsSpace(line[i])) {
            ++i;
        }
        const std::size_t start = i;
        while (i < line.size() && !IsSpace(line[i])) {
            ++i;
        }
        if (i > start) {
            words.push_back(line.substr(start, i - start));
        }
    }
    return words;
}

std::vector<std::string_view> Split(std::string_view text, char separator) {
    std::vector<std::string_view> fields;
    for (std::size_t start = 0;;) {
        const std::size_t at = text.find(separator, start);
        fields.push_back(text.substr(start, at - start));
        if (at == std::string_view::npos) {
            return fields;
        }
        start = at + 1;
    }
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && IsSpace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && IsSpace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

std::optional<Decimal> ParseDecimal(std::string_view text) {
    std::size_t i = 0;
    double value = 0;
    for (; i < text.size() && IsDigit(text[i]); ++i) {
        value = value * 10 + (text[i] - '0');
    }
    if (i == 0) {
        return std::nullopt;
    }
    std::size_t decimals = 0;
    if (i < text.size() && text[i] == '.') {
        ++i;
        double scale = 0.1;
        for (; i < text.size() && IsDigit(text[i]); ++i, ++decimals) {
            value += (text[i] - '0') * scale;
            scale /= 10;
        }
        if (decimals == 0) {
            return std::nullopt;
        }
    }
    return Decimal{value, i, decimals};
}

std::optional<double> ParsePercent(std::string_view text, std::size_t maxDecimals) {
    const auto number = ParseDecimal(text);
    if (!number || text.substr(number->length) != "%" || number->decimals > maxDecimals ||
        number->value > 100) {
        return std::nullopt;
    }
    return number->value;
}

}  // namespace bw
