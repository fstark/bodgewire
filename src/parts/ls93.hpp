#pragma once

#include <cstdint>
#include <string>

#include "sim/component.hpp"

namespace bw {

// 4-bit binary counter: ÷2 (CKA → QA) and ÷8 (CKB → QB..QD), falling edges; asynchronous R0
// (clear). Pins per datasheet: 1 CKB, 2 R01, 3 R02, 4 NC4, 5 VCC, 6 NC6, 7 NC7, 8 QC, 9 QB,
// 10 GND, 11 QD, 12 QA, 13 NC13, 14 CKA.
class Ls93 final : public Component {
public:
    explicit Ls93(std::string reference);
    static constexpr PinIndex kCkb = 0, kR01 = 1, kR02 = 2, kQC = 7, kQB = 8, kQD = 10, kQA = 11,
                              kCka = 13;

    void powerOn(PowerOnContext& ctx) override;  // QA and the ÷8 section random
    void evaluate(EvalContext& ctx) override;

private:
    bool qa_ = false;
    std::uint8_t eight_ = 0;  // ÷8 section, QD QC QB as bits 2..0
    bool unknownA_ = false;
    bool unknownEight_ = false;
};

}  // namespace bw
