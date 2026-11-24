#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "sim/time.hpp"

namespace bw {

struct KnobSetting {
    std::string reference;  // "J2"
    double turn;            // 0 (left stop) .. 1 (right stop)
    Time at = 0;            // simulated ps; 0: from power-on
};

// "J2:40%", "J2:12.5%" (0 to 100 %), or with a time "J2:87%:400ms"; error text otherwise.
std::expected<KnobSetting, std::string> ParseKnob(std::string_view text);

}  // namespace bw
