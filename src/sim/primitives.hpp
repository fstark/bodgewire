#pragma once

#include <bit>
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

// 1 → rise, 0 → fall, X/Z → the slower of the two.
constexpr Time DelayFor(Delays delays, LogicValue value) {
    if (value == LogicValue::One) {
        return delays.rise;
    }
    if (value == LogicValue::Zero) {
        return delays.fall;
    }
    return delays.rise > delays.fall ? delays.rise : delays.fall;
}

enum class GateKind : std::uint8_t { And, Or, Nand, Nor, Xor, Xnor, Not, Buffer };

namespace detail {

constexpr LogicValue AndOf(std::span<const LogicValue> inputs) {
    bool sawX = false;
    for (const LogicValue v : inputs) {
        if (v == LogicValue::Zero) {
            return LogicValue::Zero;
        }
        sawX |= v != LogicValue::One;
    }
    return sawX ? LogicValue::X : LogicValue::One;
}

constexpr LogicValue OrOf(std::span<const LogicValue> inputs) {
    bool sawX = false;
    for (const LogicValue v : inputs) {
        if (v == LogicValue::One) {
            return LogicValue::One;
        }
        sawX |= v != LogicValue::Zero;
    }
    return sawX ? LogicValue::X : LogicValue::Zero;
}

constexpr LogicValue XorOf(std::span<const LogicValue> inputs) {
    bool parity = false;
    for (const LogicValue v : inputs) {
        if (v != LogicValue::Zero && v != LogicValue::One) {
            return LogicValue::X;
        }
        parity ^= v == LogicValue::One;
    }
    return parity ? LogicValue::One : LogicValue::Zero;
}

}  // namespace detail

// Pure three-valued gate function; inputs are 0/1/X.
constexpr LogicValue GateFunction(GateKind kind, std::span<const LogicValue> inputs) {
    switch (kind) {
    case GateKind::And:
        return detail::AndOf(inputs);
    case GateKind::Or:
        return detail::OrOf(inputs);
    case GateKind::Nand:
        return Not(detail::AndOf(inputs));
    case GateKind::Nor:
        return Not(detail::OrOf(inputs));
    case GateKind::Xor:
        return detail::XorOf(inputs);
    case GateKind::Xnor:
        return Not(detail::XorOf(inputs));
    case GateKind::Not:
        return Not(inputs[0]);
    case GateKind::Buffer:
        return inputs[0];
    }
    return LogicValue::X;
}

// GateFunction over the Pins in `mask`, three-valued. Not/Buffer: mask has one bit.
constexpr LogicValue PackedGateFunction(GateKind kind, PackedInputs inputs, std::uint64_t mask) {
    const std::uint64_t ones = inputs.value & mask;
    const std::uint64_t zeros = inputs.known & ~inputs.value & mask;
    const auto andOf = [&] {
        return zeros != 0 ? LogicValue::Zero : ones == mask ? LogicValue::One : LogicValue::X;
    };
    const auto orOf = [&] {
        return ones != 0 ? LogicValue::One : zeros == mask ? LogicValue::Zero : LogicValue::X;
    };
    const auto xorOf = [&] {
        if ((ones | zeros) != mask) {
            return LogicValue::X;
        }
        return (std::popcount(ones) & 1) != 0 ? LogicValue::One : LogicValue::Zero;
    };
    const auto bufferOf = [&] {
        return ones != 0 ? LogicValue::One : zeros != 0 ? LogicValue::Zero : LogicValue::X;
    };
    switch (kind) {
    case GateKind::And:
        return andOf();
    case GateKind::Or:
        return orOf();
    case GateKind::Nand:
        return Not(andOf());
    case GateKind::Nor:
        return Not(orOf());
    case GateKind::Xor:
        return xorOf();
    case GateKind::Xnor:
        return Not(xorOf());
    case GateKind::Not:
        return Not(bufferOf());
    case GateKind::Buffer:
        return bufferOf();
    }
    return LogicValue::X;
}

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

// Pins D, CLK, Q, Q_N. Rising-edge; samples D as before the edge.
class DFlipFlop final : public Component {
public:
    DFlipFlop(std::string name, Delays clockToQ, Family family = Family::Ttl);
    static constexpr PinIndex kD = 0, kClk = 1, kQ = 2, kQN = 3;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    Delays clockToQ_;
    LogicValue q_ = LogicValue::Zero;
};

// Pin OUT. Square wave; high for period / 2, low for the remainder.
class ClockSource final : public Component {
public:
    ClockSource(std::string name, Time period, Family family = Family::Ttl);
    static constexpr PinIndex kOut = 0;

    void powerOn(PowerOnContext& ctx) override;
    void evaluate(EvalContext& ctx) override;

private:
    Time highTime_;
    Time lowTime_;
    LogicValue level_ = LogicValue::Zero;
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
