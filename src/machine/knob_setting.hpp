#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace bw {

struct KnobSetting {
    std::string reference;  // "J2"
    double turn;            // 0 (left stop) .. 1 (right stop)
};

// "J2:40%", "J2:12.5%" (0 to 100 %); error text otherwise.
std::expected<KnobSetting, std::string> ParseKnob(std::string_view text);

}  // namespace bw
