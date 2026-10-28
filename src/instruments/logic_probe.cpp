#include "instruments/logic_probe.hpp"

namespace bw {

void ProbeState::reset() {
    started_ = false;
    lastChange_ = -1;
}

void ProbeState::feed(Sample sample) {
    if (started_ && sample.value != value_) {
        lastChange_ = sample.time;
    }
    value_ = sample.value;
    floating_ = sample.floating;
    started_ = true;
}

ProbeLamps ProbeState::lamps(Time now) const {
    const bool level = started_ && !floating_;
    return {
        .high = level && value_ == LogicValue::One,
        .low = level && value_ == LogicValue::Zero,
        .pulse = lastChange_ >= 0 && now - lastChange_ < kPulseStretch,
    };
}

LogicProbe::LogicProbe(Simulator& sim, NetId net) : sim_(sim), net_(net) { sim_.record(net_); }

void LogicProbe::update() {
    const auto samples = sim_.recorder().samples(net_);
    std::size_t first = samples.size();
    while (first > 0 && samples[first - 1].time > fedUntil_) {
        --first;
    }
    for (std::size_t i = first; i < samples.size(); ++i) {
        state_.feed(samples[i]);
    }
    fedUntil_ = sim_.now();
}

ProbeLamps LogicProbe::lamps() const { return state_.lamps(sim_.now()); }

std::string FormatLamps(ProbeLamps lamps) {
    std::string text;
    const auto add = [&](bool lit, const char* name) {
        if (lit) {
            text += text.empty() ? "" : " ";
            text += name;
        }
    };
    add(lamps.high, "HIGH");
    add(lamps.low, "LOW");
    add(lamps.pulse, "PULSE");
    return text.empty() ? "-" : text;
}

}  // namespace bw
