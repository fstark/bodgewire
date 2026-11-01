#include "devices/led_glow.hpp"

#include <algorithm>

namespace bw {

void LedGlow::setAnode(Sample sample) {
    advanceTo(sample.time);
    anode_ = sample;
}

void LedGlow::setCathode(Sample sample) {
    advanceTo(sample.time);
    cathode_ = sample;
}

void LedGlow::advanceTo(Time time) {
    while (windowEnd_ <= time) {
        integrateTo(windowEnd_);
        brightness_ = static_cast<float>(litTime_) / static_cast<float>(kLedWindow);
        litTime_ = 0;
        windowEnd_ += kLedWindow;
    }
    integrateTo(time);
}

void LedGlow::reset() { *this = LedGlow{}; }

bool LedGlow::lit() const {
    return anode_.value == LogicValue::One && !anode_.floating &&
           cathode_.value == LogicValue::Zero && !cathode_.floating;
}

void LedGlow::integrateTo(Time time) {
    if (time > integrated_ && lit()) {
        litTime_ += time - integrated_;
    }
    integrated_ = std::max(integrated_, time);
}

}  // namespace bw
