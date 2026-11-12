#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Hex 32-bit static shift register (Signetics 2518), P-MOS. Pins per datasheet: 1 IN4, 2 IN5,
// 3 IN6, 4 REC, 5 VGG, 6 CLK, 7 OUT6, 8 OUT5, 9 OUT4, 10 OUT3, 11 OUT2, 12 OUT1, 13 IN1, 14 IN2,
// 15 IN3, 16 VCC. On the rise of CLK each of the six registers puts the bit at its output back
// in (REC high, "Recirculate") or takes its IN (REC low), and the next bit comes out. VGG
// (-12 V) is not connected on these Boards; no GND pin. Inputs read X as a random bit.
class ShiftRegister2518 final : public Component {
public:
    explicit ShiftRegister2518(std::string reference);
    static constexpr PinIndex kRec = 3, kClk = 5;
    static constexpr std::array<PinIndex, 6> kIn{12, 13, 14, 0, 1, 2};  // IN1..IN6
    static constexpr std::array<PinIndex, 6> kOut{11, 10, 9, 8, 7, 6};  // OUT1..OUT6
    static constexpr std::size_t kWords = 32;

    void powerOn(PowerOnContext& ctx) override;  // random words
    void evaluate(EvalContext& ctx) override;
    // The six bits `offset` places from the output (0: at OUT1-OUT6); bit i is register i + 1.
    std::uint8_t word(std::size_t offset) const { return words_[(head_ + offset) % kWords]; }

private:
    std::array<std::uint8_t, kWords> words_{};
    std::size_t head_ = 0;  // the word at the outputs
};

}  // namespace bw
