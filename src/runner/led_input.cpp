#include "runner/led_input.hpp"

#include <utility>

namespace bw {

LedInput::LedInput(Simulator& sim, std::vector<LedNets> leds)
    : sim_(sim), consumer_(std::move(leds)) {
    for (const NetId net : consumer_.nets()) {
        sim_.record(net);
    }
    consumer_.powerOn();
}

void LedInput::update() {
    const auto nets = consumer_.nets();
    for (const auto& [input, sample] : sim_.recorder().inOrder(nets, fedUntil_)) {
        consumer_.sample(nets[input], sample);
    }
    consumer_.advanceTo(sim_.now());
    fedUntil_ = sim_.now();
}

}  // namespace bw
