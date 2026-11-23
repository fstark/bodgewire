#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "sim/time.hpp"

namespace bw {

// Durations and frequencies: ParseDuration, ParseFrequencyPeriod (sim/time.hpp).
// Ohms (P5u): digits, then optionally R, k, K or M standing for the decimal point and the
// multiplier, then more digits: "470", "4R7" (5), "1k", "2k2", "47K", "1M".
std::expected<std::uint32_t, std::string> ParseOhms(std::string_view text);
// Picofarads (P5v): digits, then p, n or u standing for the decimal point and the multiplier,
// then more digits: "47p", "4p7" (5), "100n", "4n7", "1u", "2u2". The letter is required.
std::expected<std::uint64_t, std::string> ParseFarads(std::string_view text);

}  // namespace bw
