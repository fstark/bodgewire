#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace bw {

using Time = std::int64_t;  // picoseconds

inline constexpr Time kPicosPerSecond = 1'000'000'000'000;

// "30us", "10ms", "250ns", "2s" → ps, rounded to nearest; the error text if malformed or < 1 ps.
std::expected<Time, std::string> ParseDuration(std::string_view text);
// "14.31818MHz", "50Hz", "2.5kHz" → its period in ps, rounded to nearest; the error text if
// malformed or ≤ 0.
std::expected<Time, std::string> ParseFrequencyPeriod(std::string_view text);

namespace literals {

constexpr Time operator""_ps(unsigned long long v) { return static_cast<Time>(v); }
constexpr Time operator""_ns(unsigned long long v) { return static_cast<Time>(v) * 1'000; }
constexpr Time operator""_us(unsigned long long v) { return static_cast<Time>(v) * 1'000'000; }
constexpr Time operator""_ms(unsigned long long v) { return static_cast<Time>(v) * 1'000'000'000; }
constexpr Time operator""_s(unsigned long long v) { return static_cast<Time>(v) * kPicosPerSecond; }

}  // namespace literals

}  // namespace bw
