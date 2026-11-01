#pragma once

#include <atomic>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "devices/led_glow.hpp"
#include "runner/consumer.hpp"

namespace bw {

struct LedNets {
    std::string reference;  // "D1"
    NetId anode, cathode;
};

// One LedGlow per LED. brightness(): UI thread; overrides: devices thread.
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
