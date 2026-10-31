#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <span>

#include "instruments/multimeter.hpp"
#include "runner/consumer.hpp"

namespace bw {

class Runner;

// MeterState as a Consumer. touch(), reading(): UI thread; overrides: devices thread.
class MeterConsumer final : public Consumer {
public:
    void touch(Runner& runner, std::optional<NetId> net);  // as ProbeConsumer::touch
    std::optional<NetId> touched() const { return touched_; }
    std::optional<double> reading() const;  // for the touched Net; nullopt before a refresh

    std::span<const NetId> nets() const override;
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void advanceTo(Time time) override;
    void powerOff() override;

private:
    // UI thread
    std::optional<NetId> touched_;
    // shared
    std::atomic<std::int64_t> target_{-1};     // touched Net id, -1 = lifted
    std::atomic<std::uint64_t> published_{0};  // (net + 1) << 32 | float bits; 0 = none
    // devices thread
    mutable std::array<NetId, 1> nets_{};
    std::int64_t fedNet_ = -1;  // Net the state_ belongs to
    MeterState state_;
};

}  // namespace bw
