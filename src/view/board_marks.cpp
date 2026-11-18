#include "view/board_marks.hpp"

#include <format>

namespace bw {

std::string CapacitorCode(std::uint64_t picofarads) {
    if (picofarads < 100) {
        return std::to_string(picofarads);
    }
    int power = 0;
    std::uint64_t digits = picofarads;
    while (digits >= 100) {
        digits = (digits + 5) / 10;  // rounded to two significant digits
        ++power;
    }
    return std::format("{}{}", digits, power);
}

}  // namespace bw
