#pragma once

#include <string>

#include "sim/component.hpp"

namespace bw {

// 4-to-16 line decoder/demultiplexer (the PET 2001's 4 KB block decoder). Pins per datasheet
// (TI76 p. 7-171): 1-11 Y0_N-Y10_N, 12 GND, 13-17 Y11_N-Y15_N, 18 G1_N, 19 G2_N, 20 D, 21 C, 22 B,
// 23 A, 24 VCC. While G1_N and G2_N are low, the output numbered by D C B A is low; otherwise all
// are high.
class Ls154 final : public Component {
public:
    explicit Ls154(std::string reference);
    static constexpr PinIndex kG1N = 17, kG2N = 18, kD = 19, kC = 20, kB = 21, kA = 22;

    void evaluate(EvalContext& ctx) override;
};

}  // namespace bw
