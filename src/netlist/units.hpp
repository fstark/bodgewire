#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "sim/time.hpp"

namespace bw {

std::expected<Time, std::string> ParsePeriodFromFrequency(std::string_view text);  // → ps
std::expected<Time, std::string> ParseDuration(std::string_view text);  // "10ms", "250ns", "2s"
// Ohms (P5u): digits, then optionally R, k, K or M standing for the decimal point and the
// multiplier, then more digits: "470", "4R7" (5), "1k", "2k2", "47K", "1M".
std::expected<std::uint32_t, std::string> ParseOhms(std::string_view text);

}  // namespace bw
