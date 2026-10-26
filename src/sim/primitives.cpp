#include "sim/primitives.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "sim/check.hpp"

namespace bw {

namespace {

using enum LogicValue;

constexpr std::uint32_t kMaxGateInputs = 16;

LogicValue AndOf(std::span<const LogicValue> inputs) {
    bool sawX = false;
    for (const LogicValue v : inputs) {
        if (v == Zero) {
            return Zero;
        }
        sawX |= v != One;
    }
    return sawX ? X : One;
}

LogicValue OrOf(std::span<const LogicValue> inputs) {
    bool sawX = false;
    for (const LogicValue v : inputs) {
        if (v == One) {
            return One;
        }
        sawX |= v != Zero;
    }
    return sawX ? X : Zero;
}

LogicValue XorOf(std::span<const LogicValue> inputs) {
    bool parity = false;
    for (const LogicValue v : inputs) {
        if (v != Zero && v != One) {
            return X;
        }
        parity ^= v == One;
    }
    return parity ? One : Zero;
}

std::vector<PinSpec> GatePins(std::uint32_t inputCount, Family family) {
    std::vector<PinSpec> pins;
    for (std::uint32_t i = 0; i < inputCount; ++i) {
        pins.push_back({"I" + std::to_string(i), PinKind::Input, family});
    }
    pins.push_back({"Y", PinKind::Output, family});
    return pins;
}

}  // namespace

Time DelayFor(Delays delays, LogicValue value) {
    if (value == One) {
        return delays.rise;
    }
    if (value == Zero) {
        return delays.fall;
    }
    return std::max(delays.rise, delays.fall);
}

// --- Gates ---

LogicValue GateFunction(GateKind kind, std::span<const LogicValue> inputs) {
    switch (kind) {
    case GateKind::And:
        return AndOf(inputs);
    case GateKind::Or:
        return OrOf(inputs);
    case GateKind::Nand:
        return Not(AndOf(inputs));
    case GateKind::Nor:
        return Not(OrOf(inputs));
    case GateKind::Xor:
        return XorOf(inputs);
    case GateKind::Xnor:
        return Not(XorOf(inputs));
    case GateKind::Not:
        return Not(inputs[0]);
    case GateKind::Buffer:
        return inputs[0];
    }
    return X;
}

Gate::Gate(std::string name, GateKind kind, std::uint32_t inputCount, Delays delays,
           Family family)
    : Component(std::move(name), GatePins(inputCount, family)), kind_(kind),
      inputCount_(inputCount), delays_(delays) {
    const bool single = kind == GateKind::Not || kind == GateKind::Buffer;
    Check(single ? inputCount == 1 : inputCount >= 2 && inputCount <= kMaxGateInputs,
          "gate: bad input count");
    Check(delays.rise >= 1 && delays.fall >= 1, "gate: delays must be >= 1 ps");
}

void Gate::evaluate(EvalContext& ctx) {
    std::array<LogicValue, kMaxGateInputs> inputs;
    for (std::uint32_t i = 0; i < inputCount_; ++i) {
        inputs[i] = ctx.read(i);
    }
    const LogicValue value = GateFunction(kind_, std::span(inputs.data(), inputCount_));
    ctx.drive(inputCount_, value, DelayFor(delays_, value));
}

// --- TriStateBuffer ---

TriStateBuffer::TriStateBuffer(std::string name, Delays data, Time toggle, Family family)
    : Component(std::move(name), {{"A", PinKind::Input, family},
                                  {"EN", PinKind::Input, family},
                                  {"Y", PinKind::Output, family}}),
      data_(data), toggle_(toggle) {
    Check(data.rise >= 1 && data.fall >= 1 && toggle >= 1,
          "tri-state buffer: delays must be >= 1 ps");
}

void TriStateBuffer::powerOn(PowerOnContext& /*ctx*/) { outputZ_ = false; }

void TriStateBuffer::evaluate(EvalContext& ctx) {
    const LogicValue enable = ctx.read(kEn);
    if (enable == Zero) {
        ctx.drive(kY, Z, toggle_);
        outputZ_ = true;
        return;
    }
    if (enable == X) {
        ctx.drive(kY, X, toggle_);
        outputZ_ = false;
        return;
    }
    const LogicValue value = ctx.read(kA);
    ctx.drive(kY, value, outputZ_ ? toggle_ : DelayFor(data_, value));
    outputZ_ = false;
}

// --- DFlipFlop ---

DFlipFlop::DFlipFlop(std::string name, Delays clockToQ, Family family)
    : Component(std::move(name), {{"D", PinKind::Input, family},
                                  {"CLK", PinKind::Input, family},
                                  {"Q", PinKind::Output, family},
                                  {"Q_N", PinKind::Output, family}}),
      clockToQ_(clockToQ) {
    Check(clockToQ.rise >= 1 && clockToQ.fall >= 1, "flip-flop: delays must be >= 1 ps");
}

void DFlipFlop::powerOn(PowerOnContext& ctx) {
    q_ = ctx.randomBit() ? One : Zero;
    ctx.setOutput(kQ, q_);
    ctx.setOutput(kQN, Not(q_));
}

void DFlipFlop::evaluate(EvalContext& ctx) {
    if (ctx.rose(kClk)) {
        q_ = ctx.readBefore(kD);
    }
    ctx.drive(kQ, q_, DelayFor(clockToQ_, q_));
    ctx.drive(kQN, Not(q_), DelayFor(clockToQ_, Not(q_)));
}

// --- ClockSource ---

ClockSource::ClockSource(std::string name, Time period, Family family)
    : Component(std::move(name), {{"OUT", PinKind::Output, family}}), highTime_(period / 2),
      lowTime_(period - period / 2) {
    Check(period >= 2, "clock source: period must be >= 2 ps");
}

void ClockSource::powerOn(PowerOnContext& ctx) {
    level_ = ctx.randomBit() ? One : Zero;
    ctx.setOutput(kOut, level_);
}

void ClockSource::evaluate(EvalContext& ctx) {
    level_ = Not(level_);
    const Time delay = level_ == One ? lowTime_ : highTime_;  // time spent at the old level
    ctx.drive(kOut, level_, delay);
    ctx.wakeAfter(delay);
}

// --- Stimulus ---

Stimulus::Stimulus(std::string name, LogicValue initial, std::vector<Step> steps, Family family)
    : Component(std::move(name), {{"OUT", PinKind::Output, family}}), initial_(initial),
      steps_(std::move(steps)) {
    Time last = 0;
    for (const Step& step : steps_) {
        Check(step.time > last, "stimulus: steps must be sorted, distinct and at times >= 1 ps");
        last = step.time;
    }
}

void Stimulus::powerOn(PowerOnContext& ctx) {
    next_ = 0;
    ctx.setOutput(kOut, initial_);
}

// Wakes 1 ps early and drives with delay 1 ps, so the output changes exactly at step.time.
void Stimulus::evaluate(EvalContext& ctx) {
    while (next_ < steps_.size() && steps_[next_].time - 1 == ctx.now()) {
        ctx.drive(kOut, steps_[next_].value, 1);
        ++next_;
    }
    if (next_ < steps_.size()) {
        ctx.wakeAfter(steps_[next_].time - 1 - ctx.now());
    }
}

}  // namespace bw
