#include "parts/oscillator.hpp"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "sim/check.hpp"
#include "sim/simulator.hpp"
#include "text/text.hpp"

namespace bw {

namespace {

using enum LogicValue;

constexpr double kMaxPeriod = 1e18;  // ps; keeps llround in range

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

std::expected<Time, std::string> PeriodFromFrequency(std::string_view text) {
    const auto bad = [&] {
        return std::unexpected("bad frequency '" + std::string(text) +
                               "': expected a number then Hz, kHz or MHz, e.g. 14.31818MHz");
    };
    const auto number = ParseDecimal(text);
    if (!number) {
        return bad();
    }
    const double value = number->value;
    const std::string_view unit = text.substr(number->length);
    double multiplier = 0;
    if (unit == "Hz") {
        multiplier = 1;
    } else if (unit == "kHz") {
        multiplier = 1e3;
    } else if (unit == "MHz") {
        multiplier = 1e6;
    } else {
        return bad();
    }
    if (value <= 0) {
        return std::unexpected("frequency must be > 0");
    }
    const double period = 1e12 / (value * multiplier);
    if (period > kMaxPeriod) {
        return std::unexpected("frequency too low");
    }
    const Time rounded = std::llround(period);
    if (rounded < 2) {
        return std::unexpected("frequency too high");
    }
    return rounded;
}

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
