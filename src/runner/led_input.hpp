#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "runner/led_consumer.hpp"
#include "sim/simulator.hpp"
#include "sim/time.hpp"

namespace bw {

// Plugs lamps into their Nets on a Simulator its caller steps (bw run --leds), as TvInput does a
// Tv. Construct after powerOn() (it clears recordings). Never trims: whoever runs the loop calls
// sim.trimRecordings() after updating every consumer.
class LedInput {
public:
    LedInput(Simulator& sim, std::vector<LedNets> leds);
    void update();  // feeds the changes recorded since the last update, then up to sim.now()
    std::span<const LedNets> leds() const { return consumer_.leds(); }
    // 0 dark .. 1 fully lit, over the last 20 ms window closed (LedGlow); 0 before the first.
    float brightness(std::size_t led) const { return consumer_.brightness(led); }

private:
    Simulator& sim_;
    LedConsumer consumer_;
    Time fedUntil_ = -1;  // Samples up to here already fed
};

}  // namespace bw
