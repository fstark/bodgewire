#include "instruments/logic_probe.hpp"

namespace bw {

LogicProbe::LogicProbe(Simulator& sim, NetId net) : sim_(sim), net_(net) { sim_.record(net_); }

void LogicProbe::update() {
    const auto samples = sim_.recorder().samples(net_);
    std::size_t first = samples.size();
    while (first > 0 && samples[first - 1].time > fedUntil_) {
        --first;
    }
    for (std::size_t i = first; i < samples.size(); ++i) {
        if (started_) {
            lastChange_ = samples[i].time;
        } else {
            started_ = true;
        }
    }
    fedUntil_ = sim_.now();
}

ProbeLamps LogicProbe::lamps() const {
    const bool floating = sim_.isFloating(net_);
    const LogicValue value = sim_.value(net_);
    return {
        .high = !floating && value == LogicValue::One,
        .low = !floating && value == LogicValue::Zero,
        .pulse = lastChange_ >= 0 && sim_.now() - lastChange_ < kPulseStretch,
    };
}

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
