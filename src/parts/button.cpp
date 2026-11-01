#include "parts/button.hpp"

#include <utility>

#include "sim/simulator.hpp"

namespace bw {

Button::Button(std::string reference)
    : Component(std::move(reference),
                {{"1", PinKind::Output, Family::Ttl}, {"2", PinKind::Output, Family::Ttl}}) {}

void Button::attach(PinIndex signal, LogicValue rail) {
    signal_ = signal;
    rail_ = rail;
}

void Button::powerOn(PowerOnContext& ctx) {
    ctx.setOutput(0, LogicValue::Z);
    ctx.setOutput(1, LogicValue::Z);
}

void Button::evaluate(EvalContext& ctx) {
    ctx.drive(signal_, pressed() ? rail_ : LogicValue::Z, kButtonDelay);
}

}  // namespace bw
