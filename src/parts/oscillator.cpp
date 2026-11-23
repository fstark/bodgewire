#include "parts/oscillator.hpp"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "sim/check.hpp"
#include "sim/simulator.hpp"

namespace bw {

namespace {

using enum LogicValue;

std::vector<PinSpec> OscillatorPins() {
    std::vector<PinSpec> pins;
    for (int pin = 1; pin <= 14; ++pin) {
        if (pin == 7) {
            pins.push_back({"GND", PinKind::Power, Family::Ttl});
        } else if (pin == 8) {
            pins.push_back({"OUT", PinKind::Output, Family::Ttl});
        } else if (pin == 14) {
            pins.push_back({"VCC", PinKind::Power, Family::Ttl});
        } else {
            pins.push_back({"NC" + std::to_string(pin), PinKind::NoConnect, Family::Ttl});
        }
    }
    return pins;
}

}  // namespace

Oscillator::Oscillator(std::string reference, Time period)
    : Component(std::move(reference), OscillatorPins()), highTime_(period / 2),
      lowTime_(period - period / 2) {
    Check(period >= 2, "oscillator: period must be >= 2 ps");
}

void Oscillator::powerOn(PowerOnContext& ctx) {
    level_ = ctx.randomBit() ? One : Zero;
    ctx.setOutput(kOut, level_);
}

void Oscillator::evaluate(EvalContext& ctx) {
    level_ = Not(level_);
    const Time delay = level_ == One ? lowTime_ : highTime_;  // time spent at the old level
    ctx.drive(kOut, level_, delay);
    ctx.wakeAfter(delay);
}

}  // namespace bw
