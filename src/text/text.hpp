#pragma once

#include <cstddef>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Small text helpers every layer's file formats and command lines share (2026-11-23: each had its
// own copy). The lowest library: no other bw library below it.

namespace bw {

// The whole file, bytes as they are; nullopt if it cannot be opened.
std::optional<std::string> ReadTextFile(const std::filesystem::path& path);

// Space, tab, CR, LF, vertical tab or form feed (std::isspace in the C locale).
constexpr bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}
constexpr bool IsDigit(char c) { return c >= '0' && c <= '9'; }

// The words of `line`, split at runs of IsSpace; none for a blank line.
std::vector<std::string_view> SplitWords(std::string_view line);

// `text` cut at every `separator`: "a,,b" → "a", "", "b"; "" → one empty field.
std::vector<std::string_view> Split(std::string_view text, char separator);

// `text` without IsSpace at either end.
std::string_view Trim(std::string_view text);

// "N" or "N.N" at the start of `text` (no sign, no exponent).
struct Decimal {
    double value;
    std::size_t length;    // characters taken
    std::size_t decimals;  // digits after the point
};
// nullopt if `text` does not start with a digit, or its point has no digit after it ("5.").
std::optional<Decimal> ParseDecimal(std::string_view text);

// "N%" or "N.N%" from 0 to 100, at most `maxDecimals` digits after the point: the number, 0 to 100.
std::optional<double>
ParsePercent(std::string_view text,
             std::size_t maxDecimals = std::numeric_limits<std::size_t>::max());

}  // namespace bw
