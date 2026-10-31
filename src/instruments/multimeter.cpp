#include "instruments/multimeter.hpp"

#include <algorithm>

#include "instruments/volts.hpp"

namespace bw {

void MeterState::reset() {
    started_ = false;
    reading_.reset();
    area_ = 0;
}

void MeterState::feed(Sample sample) {
    if (!started_) {
        started_ = true;
        integrated_ = sample.time;
        next_ = (sample.time / kMeterRefresh + 1) * kMeterRefresh;
        windowStart_ = std::max(next_ - kMeterWindow, sample.time);
        area_ = 0;
    } else {
        advanceTo(sample.time);  // a refresh exactly at sample.time uses the old value
    }
    volts_ = LogicVolts(sample);
}

void MeterState::advanceTo(Time end) {
    if (!started_) {
        return;
    }
    while (next_ <= end) {
        refresh();
    }
    integrateTo(end);
}

void MeterState::refresh() {
    integrateTo(next_);
    reading_ = area_ / static_cast<double>(next_ - windowStart_);
    next_ += kMeterRefresh;
    windowStart_ = next_ - kMeterWindow;
    area_ = 0;
}

void MeterState::integrateTo(Time time) {
    const Time from = std::max(integrated_, windowStart_);
    if (time > from) {
        area_ += volts_ * static_cast<double>(time - from);
    }
    integrated_ = std::max(integrated_, time);
}

Multimeter::Multimeter(Simulator& sim, NetId net) : sim_(sim), net_(net) { sim_.record(net_); }

void Multimeter::update() {
    const auto samples = sim_.recorder().samples(net_);
    std::size_t first = samples.size();
    while (first > 0 && samples[first - 1].time > fedUntil_) {
        --first;
    }
    for (std::size_t i = first; i < samples.size(); ++i) {
        state_.feed(samples[i]);
    }
    state_.advanceTo(sim_.now());
    fedUntil_ = sim_.now();
}

}  // namespace bw
