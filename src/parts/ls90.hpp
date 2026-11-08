#pragma once

#include <cstdint>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Decade counter: ÷2 (CKA → QA) and ÷5 (CKB → QB..QD), falling edges; asynchronous R0 (clear)
// and R9 (set 9). Pins per datasheet: 1 CKB, 2 R01, 3 R02, 4 NC4, 5 VCC, 6 R91, 7 R92, 8 QC,
// 9 QB, 10 GND, 11 QD, 12 QA, 13 NC13, 14 CKA.
class Ls90 final : public Component {
public:
    explicit Ls90(std::string reference);
    static constexpr PinIndex kCkb = 0, kR01 = 1, kR02 = 2, kR91 = 5, kR92 = 6, kQC = 7, kQB = 8,
                              kQD = 10, kQA = 11, kCka = 13;

    void powerOn(PowerOnContext& ctx) override;  // QA and the ÷5 section (0..7) random
    void evaluate(EvalContext& ctx) override;

private:
    bool qa_ = false;
    std::uint8_t five_ = 0;  // ÷5 section, QD QC QB as bits 2..0
    bool unknownA_ = false;
    bool unknownFive_ = false;
};

}  // namespace bw
