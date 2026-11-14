#pragma once

#include <string>

#include "sim/component.hpp"

namespace bw {

// BCD-to-decimal decoder/driver, open-collector outputs (the KIM-1's keypad and digit scanner).
// Pins per datasheet: 1 Y0_N, 2 Y1_N, 3 Y2_N, 4 Y3_N, 5 Y4_N, 6 Y5_N, 7 Y6_N, 8 GND, 9 Y7_N,
// 10 Y8_N, 11 Y9_N, 12 D, 13 C, 14 B, 15 A, 16 VCC. The output numbered by D C B A (0-9) is
// pulled low; the others are released. Codes 10-15 release all ten.
class Ttl145 final : public Component {
public:
    explicit Ttl145(std::string reference);
    static constexpr PinIndex kD = 11, kC = 12, kB = 13, kA = 14;

    void evaluate(EvalContext& ctx) override;
};

}  // namespace bw
