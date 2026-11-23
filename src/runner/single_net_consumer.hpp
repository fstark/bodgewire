#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <span>

#include "runner/consumer.hpp"

namespace bw {

class Runner;

// A Consumer that watches the one Net an instrument's tip touches (the Logic Probe, the
// Multimeter) and feeds its Samples to a `State` (reset(), feed(Sample)), started afresh whenever
// the Net changes. touch(), touched(): UI thread; the overrides and follow(): devices thread. The
// derived class publishes in advanceTo() and reads back on the UI thread. Instantiated for
// ProbeState and MeterState (single_net_consumer.cpp).
template <class State>
class SingleNetConsumer : public Consumer {
public:
    // Watches `net` (nullopt: lifted), unwatches the previous one. Same Net: no-op.
    void touch(Runner& runner, std::optional<NetId> net);
    std::optional<NetId> touched() const { return touched_; }

    std::span<const NetId> nets() const override;  // the touched Net, or none
    void powerOn() override;
    void sample(NetId net, Sample sample) override;
    void powerOff() override;  // also clears published_

protected:
    // The touched Net's id, -1 when lifted; state_ reset if it changed since the last call.
    std::int64_t follow();

    std::atomic<std::uint64_t> published_{0};  // the derived class's encoding; 0 = nothing
    State state_;                              // devices thread

private:
    // UI thread
    std::optional<NetId> touched_;
    // shared
    std::atomic<std::int64_t> target_{-1};  // touched Net id, -1 = lifted
    // devices thread
    mutable std::array<NetId, 1> nets_{};  // nets() view of target_
    std::int64_t fedNet_ = -1;             // Net the state_ belongs to
};

}  // namespace bw
