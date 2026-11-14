#pragma once

#include <cstdint>
#include <string>

#include "sim/component.hpp"

namespace bw {

enum class DigitCommon : std::uint8_t { Anode, Cathode };

// Red 0.56" 7-segment digit, common anode (LTS-6960HR pinout) or common cathode (LTS-6780's, the
// same pads). Pin index = KiCad pad − 1: 1 E, 2 D, 3 CA1 or CC1, 4 C, 5 DP, 6 B, 7 A, 8 CA2 or
// CC2, 9 F, 10 G. Passive like Led: every Pin a Family::Mos load, no behaviour; its lamps are in
// the catalogue.
class Digit final : public Component {
public:
    explicit Digit(std::string reference, DigitCommon common = DigitCommon::Anode);
    void evaluate(EvalContext& ctx) override;
};

}  // namespace bw
