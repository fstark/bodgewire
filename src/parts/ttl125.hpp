#pragma once

#include <array>
#include <string>

#include "sim/component.hpp"

namespace bw {

// Quad bus buffer, tri-state, one active-low enable each (the KIM-1's RAM output buffers). Pins
// per datasheet: 1 1OE_N, 2 1A, 3 1Y, 4 2OE_N, 5 2A, 6 2Y, 7 GND, 8 3Y, 9 3A, 10 3OE_N, 11 4Y,
// 12 4A, 13 4OE_N, 14 VCC. Each Y follows its A while its OE_N is low, else Z.
class Ttl125 final : public Component {
public:
    explicit Ttl125(std::string reference);

    void powerOn(PowerOnContext& ctx) override;  // outputs released
    void evaluate(EvalContext& ctx) override;

private:
    // Last value scheduled per buffer: picks the data, enable or disable delay (as the 74LS244).
    std::array<LogicValue, 4> driven_{};
};

}  // namespace bw
