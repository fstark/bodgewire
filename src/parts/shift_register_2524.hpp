#pragma once

#include <bitset>
#include <cstddef>
#include <string>

#include "sim/component.hpp"
#include "sim/time.hpp"

namespace bw {

// 512-bit dynamic recirculating shift register (Signetics 2524), P-MOS. Pins per datasheet:
// 1 PHI2, 2 OUT, 3 READ, 4 VDD, 5 WRITE, 6 IN, 7 PHI1, 8 VCC. Two clocks pulsed low in turn: on
// the rise of PHI2 the bit at the output is rewritten from IN if WRITE is high, else kept
// (recirculated); on the rise of PHI1 the next bit comes to OUT, or 0 while READ is low. A
// written bit reaches OUT only when it comes round. Dynamic: if either clock has not risen for
// k2524Hold, every bit fades to the same value, 1 if PHI2 rose last and 0 if PHI1 did, and stays
// so until written. OUT holds its last value while the clocks are stopped. VDD (-5 V) is not
// connected on these Boards; no GND pin. Inputs read X as a random bit.
class ShiftRegister2524 final : public Component {
public:
    explicit ShiftRegister2524(std::string reference);
    static constexpr PinIndex kPhi2 = 0, kOut = 1, kRead = 2, kWrite = 4, kIn = 5, kPhi1 = 6;
    static constexpr std::size_t kBits = 512;

    void powerOn(PowerOnContext& ctx) override;  // random bits
    void evaluate(EvalContext& ctx) override;
    bool bit(std::size_t offset) const { return bits_[(head_ + offset) % kBits]; }  // 0: at OUT

private:
    std::bitset<kBits> bits_;
    std::size_t head_ = 0;  // the bit at the output
    bool out_ = false;      // the output stage: bits_[head_] as of the last PHI1 rise
    Time lastPhi1_ = 0, lastPhi2_ = 0;
    bool faded_ = false;  // faded since a clock last stopped: not again until both run
};

}  // namespace bw
