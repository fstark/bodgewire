#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "devices/led_glow.hpp"
#include "runner/consumer.hpp"

namespace bw {

// One lamp of a Part (the LED itself, or a segment of a digit).
struct LedNets {
    std::string reference;                         // "D1", "DS1"
    std::size_t lamp;                              // index into the Part's catalogue lamps
    std::array<std::optional<NetId>, 2> anodes;    // at least one
    std::array<std::optional<NetId>, 2> cathodes;  // at least one
};

// One LedGlow per lamp. brightness(): UI thread; overrides: devices thread.
class LedConsumer final : public Consumer {
public:
    explicit LedConsumer(std::vector<LedNets> leds);
    std::span<const LedNets> leds() const { return leds_; }
    float brightness(std::size_t led) const;

    std::span<const NetId> nets() const override { return nets_; }  // distinct A/K Nets
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void advanceTo(Time time) override;
    void powerOff() override;

private:
    void publishDark();

    std::vector<LedNets> leds_;
    std::vector<NetId> nets_;
    std::vector<LedGlow> glows_;                 // devices thread
    std::vector<std::atomic<float>> published_;  // sized once in the constructor
};

}  // namespace bw
