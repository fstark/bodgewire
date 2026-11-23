#pragma once

#include <cstdint>
#include <span>

#include "sim/logic.hpp"

namespace bw {

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

}  // namespace bw
