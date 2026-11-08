#pragma once

#include <string>

#include "sim/component.hpp"

namespace bw {

// Red 0.56" common-anode 7-segment digit (LTS-6960HR pinout). Pin index = KiCad pad − 1:
// 1 E, 2 D, 3 CA1, 4 C, 5 DP, 6 B, 7 A, 8 CA2, 9 F, 10 G. Passive like Led: every Pin a
// Family::Mos load, no behaviour; its lamps are in the catalogue.
class Digit final : public Component {
public:
    explicit Digit(std::string reference);
    void evaluate(EvalContext& ctx) override;
};

}  // namespace bw
