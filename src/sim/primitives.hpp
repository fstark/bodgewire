#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "sim/component.hpp"
#include "sim/logic.hpp"
#include "sim/time.hpp"

namespace bw {

struct Delays {
    Time rise;  // tPLH
    Time fall;  // tPHL
};

enum class GateKind : std::uint8_t { And, Or, Nand, Nor, Xor, Xnor, Not, Buffer };

// Pure three-valued gate function; inputs are 0/1/X.
LogicValue GateFunction(GateKind kind, std::span<const LogicValue> inputs);

// Pins I0..I(n-1), then Y. Not/Buffer: n = 1. Others: 2 ≤ n ≤ 16.
class Gate final : public Component {
public:
    Gate(std::string name, GateKind kind, std::uint32_t inputCount, Delays delays,
         Family family = Family::Ttl);
    PinIndex output() const { return inputCount_; }

    void evaluate(EvalContext& ctx) override;

private:
    GateKind kind_;
    std::uint32_t inputCount_;
    Delays delays_;
};

// Pins A, EN, Y. EN = 0 → Y = Z after `toggle`; data changes use `data`.
class TriStateBuffer final : public Component {
public:
    TriStateBuffer(std::string name, Delays data, Time toggle, Family family = Family::Ttl);
    static constexpr PinIndex kA = 0, kEn = 1, kY = 2;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    Delays data_;
    Time toggle_;
    bool outputZ_ = false;  // last value driven on Y was Z
};

// Pin OUT. Test and benchmark stimulus: initial value, then steps at absolute times ≥ 1 ps.
// Values may be Z (tri-state driver); family selects contention rules.
class Stimulus final : public Component {
public:
    struct Step {
        Time time;
        LogicValue value;
    };
    Stimulus(std::string name, LogicValue initial, std::vector<Step> steps,
             Family family = Family::Ttl);  // steps sorted, distinct times
    static constexpr PinIndex kOut = 0;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    LogicValue initial_;
    std::vector<Step> steps_;
    std::size_t next_ = 0;  // index of the next step
};

}  // namespace bw
