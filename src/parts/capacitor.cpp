#include "parts/capacitor.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "sim/simulator.hpp"

namespace bw {

namespace {

std::vector<PinSpec> CapacitorPins(PinIndex input) {
    std::vector<PinSpec> pins{PinSpec{"1", PinKind::Output, Family::Ttl},
                              PinSpec{"2", PinKind::Output, Family::Ttl}};
    pins[input].kind = PinKind::Input;
    return pins;
}

}  // namespace

Capacitor::Capacitor(std::string reference, PinIndex input, Time width)
    : Component(std::move(reference), CapacitorPins(input)), input_(input),
      width_(std::max<Time>(width, 1)) {}

void Capacitor::powerOn(PowerOnContext& /*ctx*/) { releaseAt_ = 0; }

void Capacitor::evaluate(EvalContext& ctx) {
    const LogicValue now = ctx.read(input_);
    if (now != ctx.readBefore(input_)) {
        ctx.drive(output(), now, kCouplingDelay);
        releaseAt_ = ctx.now() + width_;
        ctx.wakeAfter(width_);
    } else if (ctx.now() >= releaseAt_) {
        ctx.drive(output(), LogicValue::Z, kCouplingDelay);
    }
}

}  // namespace bw
