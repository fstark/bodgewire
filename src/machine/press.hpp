#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "sim/time.hpp"

namespace bw {

struct ButtonPress {
    std::string reference;  // "SW1"
    Time down, up;          // simulated time, ps; down < up
};

// "SW1:200ms:700ms" (durations as ParseDuration); error text otherwise.
std::expected<ButtonPress, std::string> ParsePress(std::string_view text);

}  // namespace bw
