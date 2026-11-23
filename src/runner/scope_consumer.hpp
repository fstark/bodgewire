#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

#include "instruments/oscilloscope.hpp"
#include "runner/channel_nets.hpp"
#include "runner/consumer.hpp"
#include "runner/triple_buffer.hpp"

namespace bw {

class Runner;

struct ScopeConfig {
    std::array<std::optional<NetId>, kScopeChannels> nets;  // electrical Net per Hook
    ScopeSettings settings;
    bool operator==(const ScopeConfig&) const = default;
};

// ScopeState as a Consumer. configure(), status(), sweeps(): UI thread; overrides: devices.
class ScopeConsumer final : public Consumer {
public:
    void configure(Runner& runner, ScopeConfig config);  // re-watches changed Channels only
    SweepKind status() const { return status_.load(std::memory_order_acquire); }
    TripleBuffer<Sweep>& sweeps() { return sweeps_; }

    std::span<const NetId> nets() const override;  // distinct hooked Nets
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void seed(NetId net, Sample sample) override;
    void advanceTo(Time time) override;
    void powerOff() override;

private:
    void adoptNets() const;  // devices: pending Nets → channels_
    void adopt();            // devices: pending config → state_, from now_

    // UI thread
    ScopeConfig sent_;
    // shared
    mutable std::mutex mutex_;
    ScopeConfig pending_;  // guarded by mutex_
    std::atomic<std::uint64_t> generation_{0};
    std::atomic<SweepKind> status_{SweepKind::None};
    TripleBuffer<Sweep> sweeps_;
    // devices thread
    mutable std::uint64_t netsGeneration_ = 0;
    mutable ChannelNets<std::uint8_t> channels_;
    std::uint64_t configGeneration_ = 0;
    ScopeConfig applied_;
    Time now_ = 0;  // last slice end; 0 after power-on
    std::uint64_t published_ = 0;
    ScopeState state_;
};

}  // namespace bw
