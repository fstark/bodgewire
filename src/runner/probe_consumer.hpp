#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <span>

#include "instruments/logic_probe.hpp"
#include "runner/consumer.hpp"

namespace bw {

class Runner;

// The Logic Probe as a Consumer. touch(), lamps(): UI thread; Consumer overrides: devices thread.
class ProbeConsumer final : public Consumer {
public:
    // Watches `net` (nullopt: lifted), unwatches the previous one. Same Net: no-op.
    void touch(Runner& runner, std::optional<NetId> net);
    std::optional<NetId> touched() const { return touched_; }
    ProbeLamps lamps() const;  // latest completed slice, for the touched Net; dark otherwise

    std::span<const NetId> nets() const override;  // the touched Net, or none
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void advanceTo(Time time) override;
    void powerOff() override;

private:
    // UI thread
    std::optional<NetId> touched_;
    // shared
    std::atomic<std::int64_t> target_{-1};     // touched Net id, -1 = lifted
    std::atomic<std::uint64_t> published_{0};  // net id << 32 | kValid | lamp bits
    // devices thread
    mutable std::array<NetId, 1> nets_{};  // nets() view of target_
    std::int64_t fedNet_ = -1;             // Net the state_ belongs to
    ProbeState state_;
};

}  // namespace bw
