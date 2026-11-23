#include "parts/paddle.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "parts/connector.hpp"
#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

}  // namespace

Paddle::Paddle(std::string reference, Time shortest, Time longest, bool reversed)
    : Component(std::move(reference), ConnectorPins("PADDLE")), shortest_(shortest),
      longest_(longest), reversed_(reversed) {
    Check(shortest >= 1 && shortest < longest, "paddle: need 1 ps <= shortest < longest");
}

void Paddle::setKnob(double turn) { knob_ = std::clamp(turn, 0.0, 1.0); }

Time Paddle::pulseLength() const {
    const Time turn = std::llround(knob() * static_cast<double>(longest_ - shortest_));
    return reversed_ ? longest_ - turn : shortest_ + turn;
}

void Paddle::powerOn(PowerOnContext& ctx) {
    busy_ = false;
    endsAt_ = 0;
    ctx.setOutput(kOut, Zero);
}

void Paddle::evaluate(EvalContext& ctx) {
    const bool ending = busy_ && ctx.now() >= endsAt_;
    if (ending) {
        busy_ = false;
    }
    const bool started = !busy_ && ctx.fell(kTrigN);
    if (started) {
        const Time length = pulseLength();
        busy_ = true;
        endsAt_ = ctx.now() + length;
        ctx.wakeAfter(length);
    }
    if (ending || started) {
        ctx.drive(kOut, busy_ ? One : Zero, kPaddleDelay);
    }
}

}  // namespace bw
