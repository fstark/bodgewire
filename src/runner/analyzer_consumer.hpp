#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "instruments/logic_analyzer.hpp"
#include "runner/consumer.hpp"
#include "runner/triple_buffer.hpp"

namespace bw {

class Runner;

// Everything the UI thread decides; sent whole on every change.
struct AnalyzerConfig {
    std::array<std::optional<NetId>, kChannels> nets;  // electrical Net per Clip
    std::array<std::string, kChannels> labels;
    AnalyzerSettings settings;
    std::uint64_t arms = 0;  // bumped by each Arm
    bool armed = false;      // false after Stop
};

// AnalyzerState as a Consumer. configure(), phase(), captures(): UI thread; overrides: devices.
class AnalyzerConsumer final : public Consumer {
public:
    void configure(Runner& runner, AnalyzerConfig config);  // watches new Nets, unwatches old
    AnalyzerPhase phase() const;  // the devices' phase once they applied the last configure()
    TripleBuffer<Capture>& captures() { return captures_; }  // completed Captures

    std::span<const NetId> nets() const override;  // distinct clipped Nets
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void seed(NetId net, Sample sample) override { this->sample(net, sample); }
    void advanceTo(Time time) override;
    void powerOff() override;

private:
    void adoptNets() const;  // devices: pending Nets → nets_, masks_
    void adopt();            // devices: pending config → state_, from now_
    void publishPhase();     // devices: state_ phase with configGeneration_

    // UI thread
    std::array<std::optional<NetId>, kChannels> watched_;
    AnalyzerConfig sent_;  // last configure(), for the UI-side phase
    std::uint64_t sentGeneration_ = 0;
    AnalyzerPhase expected_ = AnalyzerPhase::Idle;  // phase() until sentGeneration_ is applied
    // shared
    mutable std::mutex mutex_;
    AnalyzerConfig pending_;                    // guarded by mutex_
    std::atomic<std::uint64_t> generation_{0};  // bumped with pending_, under mutex_
    std::atomic<std::uint64_t> phase_{0};       // applied generation << 8 | AnalyzerPhase
    TripleBuffer<Capture> captures_;
    // devices thread
    mutable std::uint64_t netsGeneration_ = 0;
    mutable std::vector<NetId> nets_;         // distinct
    mutable std::vector<ChannelMask> masks_;  // Channels per nets_ entry
    std::uint64_t configGeneration_ = 0;
    Time now_ = 0;  // last slice end; 0 after power-on
    AnalyzerConfig applied_;
    std::uint64_t published_ = 0;  // Capture serial last published
    AnalyzerState state_;
};

}  // namespace bw
