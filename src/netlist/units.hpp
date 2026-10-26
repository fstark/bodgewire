#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "sim/time.hpp"

namespace bw {

std::expected<Time, std::string> ParsePeriodFromFrequency(std::string_view text);  // → ps
std::expected<Time, std::string> ParseDuration(std::string_view text);  // "10ms", "250ns", "2s"

}  // namespace bw
