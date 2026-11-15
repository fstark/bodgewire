#pragma once

#include <cstdint>
#include <vector>

#include "sim/circuit.hpp"
#include "sim/event_queue.hpp"
#include "sim/ids.hpp"
#include "sim/logic.hpp"
#include "sim/recorder.hpp"
#include "sim/rng.hpp"
#include "sim/time.hpp"

namespace bw {

struct SimStats {
    std::uint64_t appliedEvents = 0;  // output changes actually applied
    std::uint64_t staleEvents = 0;    // cancelled by inertial delay, skipped
    std::uint64_t wakes = 0;
    std::uint64_t evaluations = 0;
};

// Runtime state of a finalized Circuit; everything here is reset by powerOn().
class Simulator {
public:
    explicit Simulator(Circuit& circuit);  // Check()s the Circuit is finalized
    void powerOn(std::uint64_t seed);      // S4
    void runUntil(Time end);
    void wake(ComponentId component, Time delay = 0);  // evaluate at now() + delay
    void runFor(Time duration) { runUntil(now_ + duration); }
    Time now() const { return now_; }
    LogicValue value(NetId net) const { return nets_[ToIndex(net)].value; }
    bool isFloating(NetId net) const { return nets_[ToIndex(net)].floating; }
    void record(NetId net);  // from now on; kept until the next powerOn
    void stopRecording(NetId net) { recorder_.stop(net); }
    const Recorder& recorder() const { return recorder_; }
    void trimRecordings() {  // bounded memory on long runs; consumers must be up to date
        for (const NetId net : recorder_.nets()) {
            recorder_.trim(net);
        }
    }
    const SimStats& stats() const { return stats_; }

private:
    friend class EvalContext;
    friend class PowerOnContext;

    struct NetState {
        Time changedAt;
        LogicValue value, previous, lastDriven;
        bool floating, dirty;
        bool networked;  // has a network Pin (Switched or Joined): see External
    };
    static_assert(sizeof(NetState) == 16);

    struct OutputState {
        std::uint32_t generation;  // 29 bits, wraps; bumped on cancellation
        LogicValue current, pending;
        bool hasPending;
    };
    static_assert(sizeof(OutputState) == 8);

    // A network Pin's view of its Net: the drives but its own, and when they last changed (P5i,
    // P5u). A Net with a Switched and a Joined Pin has one for each.
    struct External {
        Resolver drives;
        Time changedAt;
    };

    void markNetDirty(NetId net);
    Resolver externalDrives(NetId net, PinId own) const;
    void updateExternal(NetId net, PinId own, External& external);
    void markComponentDirty(ComponentId component);
    Resolution resolveNet(NetId net) const;
    void scheduleWake(ComponentId component, Time delay);
    const NetState& netOfPin(PinId pin) const { return nets_[ToIndex(pinNet_[ToIndex(pin)])]; }

    Circuit& circuit_;
    std::vector<NetState> nets_;
    std::vector<NetId> pinNet_;  // each Pin's Net, copied from the Circuit: a read is two loads
    std::vector<OutputState> outputs_;
    std::vector<External> external_;   // per Net, its first network Pin's; where networked
    std::vector<External> external2_;  // per Net, its second network Pin's, if any
    std::vector<std::uint8_t> componentDirty_;
    std::vector<NetId> dirtyNets_;
    std::vector<ComponentId> dirtyComponents_;
    EventQueue queue_;
    Rng rng_;
    Recorder recorder_;
    SimStats stats_;
    Time now_ = 0;
};

// --- EvalContext hot path ---

inline LogicValue EvalContext::read(PinIndex pin) const { return sim_.netOfPin(pinId(pin)).value; }

inline LogicValue EvalContext::readBefore(PinIndex pin) const {
    const auto& s = sim_.netOfPin(pinId(pin));
    return s.changedAt == sim_.now_ ? s.previous : s.value;
}

inline bool EvalContext::rose(PinIndex pin) const {
    const auto& s = sim_.netOfPin(pinId(pin));
    return s.changedAt == sim_.now_ && s.previous == LogicValue::Zero && s.value == LogicValue::One;
}

inline bool EvalContext::fell(PinIndex pin) const {
    const auto& s = sim_.netOfPin(pinId(pin));
    return s.changedAt == sim_.now_ && s.previous == LogicValue::One && s.value == LogicValue::Zero;
}

inline const Resolver& EvalContext::external(PinIndex pin) const {
    const PinId id = pinId(pin);
    const NetId net = sim_.pinNet_[ToIndex(id)];
    const auto& externals =
        sim_.circuit_.secondNetworkPin(net) == id ? sim_.external2_ : sim_.external_;
    return externals[ToIndex(net)].drives;
}

inline bool EvalContext::externalChanged(PinIndex pin) const {
    const PinId id = pinId(pin);
    const NetId net = sim_.pinNet_[ToIndex(id)];
    const auto& externals =
        sim_.circuit_.secondNetworkPin(net) == id ? sim_.external2_ : sim_.external_;
    return externals[ToIndex(net)].changedAt == sim_.now_;
}

inline void EvalContext::drive(PinIndex pin, LogicValue value, Time delay) {
    const PinId id = pinId(pin);
    const auto& o = sim_.outputs_[ToIndex(id)];
    if (value != (o.hasPending ? o.pending : o.current)) {
        driveChanged(id, value, delay);
    }
}

}  // namespace bw
