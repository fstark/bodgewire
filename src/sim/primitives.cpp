#include "sim/primitives.hpp"

#include <utility>

#include "sim/check.hpp"

namespace bw {

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
