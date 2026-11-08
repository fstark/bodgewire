#include "runner/led_consumer.hpp"

#include <algorithm>
#include <utility>

namespace bw {

LedConsumer::LedConsumer(std::vector<LedNets> leds)
    : leds_(std::move(leds)), glows_(leds_.size()), published_(leds_.size()) {
    const auto add = [this](NetId net) {
        if (std::ranges::find(nets_, net) == nets_.end()) {
            nets_.push_back(net);
        }
    };
    for (const LedNets& led : leds_) {
        for (const std::optional<NetId>& anode : led.anodes) {
            if (anode) {
                add(*anode);
            }
        }
        add(led.cathode);
    }
}

float LedConsumer::brightness(std::size_t led) const {
    return published_[led].load(std::memory_order_relaxed);
}

void LedConsumer::powerOn() {
    for (LedGlow& glow : glows_) {
        glow.reset();
    }
    publishDark();
}

void LedConsumer::sample(NetId net, Sample sample) {
    for (std::size_t i = 0; i < leds_.size(); ++i) {
        for (std::size_t a = 0; a < leds_[i].anodes.size(); ++a) {
            if (leds_[i].anodes[a] == net) {
                glows_[i].setAnode(sample, a);
            }
        }
        if (leds_[i].cathode == net) {
            glows_[i].setCathode(sample);
        }
    }
}

void LedConsumer::advanceTo(Time time) {
    for (std::size_t i = 0; i < glows_.size(); ++i) {
        glows_[i].advanceTo(time);
        published_[i].store(glows_[i].brightness(), std::memory_order_relaxed);
    }
}

void LedConsumer::powerOff() { publishDark(); }

void LedConsumer::publishDark() {
    for (std::atomic<float>& value : published_) {
        value.store(0.0f, std::memory_order_relaxed);
    }
}

}  // namespace bw
