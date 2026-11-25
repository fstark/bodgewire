#include "sim/simulator.hpp"

#include <algorithm>
#include <cassert>

#include "sim/check.hpp"
#include "sim/component.hpp"

namespace bw {

namespace {

PinId Offset(PinId first, PinIndex pin) { return PinId{ToIndex(first) + pin}; }

}  // namespace

// --- EvalContext ---

Time EvalContext::now() const { return sim_.now_; }

bool EvalContext::readBit(PinIndex pin) {
    const LogicValue value = read(pin);
    if (value == LogicValue::X) {
        return sim_.rng_.nextBit();
    }
    return value == LogicValue::One;
}

bool EvalContext::readBitBefore(PinIndex pin) {
    const LogicValue value = readBefore(pin);
    if (value == LogicValue::X) {
        return sim_.rng_.nextBit();
    }
    return value == LogicValue::One;
}

void EvalContext::driveChanged(PinId id, LogicValue value, Time delay) {
    assert(delay >= 1);
    assert(IsDriver(sim_.circuit_.pinInfo(id).kind));
    auto& o = sim_.outputs_[ToIndex(id)];
    if (o.hasPending) {
        o.generation = (o.generation + 1) & kGenerationMask;
        o.hasPending = false;
    }
    if (value != o.current) {
        sim_.queue_.push({sim_.now_ + delay, ToIndex(id), MakeStamp(o.generation, false, value)});
        o.pending = value;
        o.hasPending = true;
    }
}

void EvalContext::wakeAfter(Time delay) { sim_.scheduleWake(component_, delay); }

void EvalContext::wake(ComponentId other, Time delay) { sim_.scheduleWake(other, delay); }

// --- PowerOnContext ---

bool PowerOnContext::randomBit() { return sim_.rng_.nextBit(); }

void PowerOnContext::setOutput(PinIndex pin, LogicValue value) {
    const PinId id = Offset(firstPin_, pin);
    assert(IsDriver(sim_.circuit_.pinInfo(id).kind));
    sim_.outputs_[ToIndex(id)].current = value;
}

// --- Simulator ---

Simulator::Simulator(Circuit& circuit) : circuit_(circuit) {
    Check(circuit.isFinalized(), "simulator: circuit not finalized");
    nets_.resize(circuit.netCount(), NetState{-1, LogicValue::Zero, LogicValue::Zero,
                                              LogicValue::Zero, false, false, false});
    external_.resize(circuit.netCount(), External{Resolver{}, -1});
    external2_.resize(circuit.netCount(), External{Resolver{}, -1});
    for (std::uint32_t n = 0; n < nets_.size(); ++n) {
        nets_[n].networked = circuit.networkPin(NetId{n}) != Circuit::kNoPin;
    }
    outputs_.resize(circuit.pinCount(), OutputState{0, LogicValue::Zero, LogicValue::Zero, false});
    componentDirty_.resize(circuit.components().size(), 0);
    pinNet_.reserve(circuit.pinCount());
    for (std::uint32_t pin = 0; pin < circuit.pinCount(); ++pin) {
        pinNet_.push_back(circuit.netOf(PinId{pin}));
    }
    fanOut_.assign(circuit.fanOutEntries().begin(), circuit.fanOutEntries().end());
    fanOutStart_.assign(circuit.fanOutStarts().begin(), circuit.fanOutStarts().end());
    pinQuiet_.resize(circuit.pinCount(), 0);
    driving_.resize(circuit.netCount(), Driving{0, 0});
}

void Simulator::setQuiet(PinId pin, bool on) {
    std::uint8_t& quiet = pinQuiet_[ToIndex(pin)];
    if (quiet == static_cast<std::uint8_t>(on)) {
        return;
    }
    const std::uint32_t entry = circuit_.fanOutIndex(pin);
    if (entry == Circuit::kNoFanOut) {
        return;  // wakes nobody anyway
    }
    quiet = static_cast<std::uint8_t>(on);
    fanOut_[entry].pins = static_cast<std::uint8_t>(fanOut_[entry].pins + (on ? -1 : 1));
}

void Simulator::markNetDirty(NetId net) {
    NetState& s = nets_[ToIndex(net)];
    if (!s.dirty) {
        s.dirty = true;
        dirtyNets_.push_back(net);
    }
}

void Simulator::markComponentDirty(ComponentId component) {
    std::uint8_t& dirty = componentDirty_[ToIndex(component)];
    if (dirty == 0) {
        dirty = 1;
        dirtyComponents_.push_back(component);
    }
}

Resolver Simulator::externalDrives(NetId net, PinId own) const {
    Resolver resolver;
    resolver.add(circuit_.tieOf(net));
    // From how many Drivers are not at Z (driving_, P5z): with none of the others, nothing to
    // add (a Z drives nothing, whatever its Pin's kind); with one, the sum names it.
    const Driving& d = driving_[ToIndex(net)];
    const bool ownDrives = outputs_[ToIndex(own)].current != LogicValue::Z;
    const std::uint32_t others = d.count - (ownDrives ? 1 : 0);
    if (others == 0) {
        return resolver;
    }
    if (others == 1) {
        const PinId pin{static_cast<std::uint32_t>(d.pinSum - (ownDrives ? ToIndex(own) : 0))};
        const PinInfo& info = circuit_.pinInfo(pin);
        resolver.add(DriveOf(info.kind, info.family, outputs_[ToIndex(pin)].current));
        return resolver;
    }
    for (const PinId pin : circuit_.drivers(net)) {
        if (pin != own) {
            const PinInfo& info = circuit_.pinInfo(pin);
            resolver.add(DriveOf(info.kind, info.family, outputs_[ToIndex(pin)].current));
        }
    }
    return resolver;
}

// Woken by the other drives only: a network Pin's own drive never wakes it (no echo).
void Simulator::updateExternal(NetId net, PinId own, External& external) {
    const Resolver drives = externalDrives(net, own);
    if (drives != external.drives) {
        external = {drives, now_};
        changedNetworkPins_.push_back(own);
        markComponentDirty(circuit_.pinInfo(own).owner);
    }
}

Resolution Simulator::resolveNet(NetId net) const {
    const NetState& state = nets_[ToIndex(net)];
    if (state.networked && circuit_.secondNetworkPin(net) == Circuit::kNoPin) {
        // external_ is up to date: add the network Pin's own drive
        Resolver resolver = external_[ToIndex(net)].drives;
        const PinId own = circuit_.networkPin(net);
        const PinInfo& info = circuit_.pinInfo(own);
        resolver.add(DriveOf(info.kind, info.family, outputs_[ToIndex(own)].current));
        return resolver.finish(circuit_.hasTtlInput(net), state.lastDriven);
    }
    const PinId single = circuit_.singleDriver(net);
    if (single != Circuit::kNoPin) {
        const LogicValue value = outputs_[ToIndex(single)].current;
        if (value != LogicValue::Z) {
            return {value, false};  // what Resolver gives for one Strong drive and no tie
        }
    }
    if (circuit_.isSharedNet(net)) {
        // What Resolver gives with no Drive (floating) or one Strong Drive and no tie.
        const Driving& d = driving_[ToIndex(net)];
        if (d.count == 0) {
            return {circuit_.hasTtlInput(net) ? LogicValue::One : state.lastDriven, true};
        }
        if (d.count == 1) {
            return {outputs_[d.pinSum].current, false};
        }
    }
    Resolver resolver;
    resolver.add(circuit_.tieOf(net));
    for (const PinId pin : circuit_.drivers(net)) {
        const PinInfo& info = circuit_.pinInfo(pin);
        resolver.add(DriveOf(info.kind, info.family, outputs_[ToIndex(pin)].current));
    }
    return resolver.finish(circuit_.hasTtlInput(net), nets_[ToIndex(net)].lastDriven);
}

void Simulator::scheduleWake(ComponentId component, Time delay) {
    assert(delay >= 1);
    queue_.push({now_ + delay, ToIndex(component), MakeStamp(0, true, LogicValue::Zero)});
}

void Simulator::wake(ComponentId component, Time delay) {
    Check(ToIndex(component) < componentDirty_.size(), "simulator: unknown component");
    Check(delay >= 0, "simulator: wake into the past");
    queue_.push({now_ + delay, ToIndex(component), MakeStamp(0, true, LogicValue::Zero)});
}

void Simulator::record(NetId net) {
    Check(ToIndex(net) < nets_.size(), "simulator: unknown net");
    recorder_.start(net, {now_, value(net), isFloating(net)});
}

void Simulator::powerOn(std::uint64_t seed) {
    queue_.clear();
    now_ = 0;
    rng_ = Rng(seed);
    recorder_.clear();
    stats_ = {};
    dirtyNets_.clear();
    dirtyComponents_.clear();
    changedNetworkPins_.clear();
    std::fill(componentDirty_.begin(), componentDirty_.end(), 0);
    // Every Pin loud, before the first evaluations below may quiet some.
    std::ranges::copy(circuit_.fanOutEntries(), fanOut_.begin());
    std::ranges::fill(pinQuiet_, 0);

    for (std::uint32_t p = 0; p < outputs_.size(); ++p) {
        OutputState& o = outputs_[p];
        o = {0, LogicValue::Zero, LogicValue::Zero, false};
        const PinKind kind = circuit_.pinInfo(PinId{p}).kind;
        if (kind == PinKind::Bidirectional || IsNetworkPin(kind) || kind == PinKind::PullUp) {
            o.current = LogicValue::Z;  // released until the first evaluation; no random draw
        } else if (IsDriver(kind)) {
            o.current = rng_.nextBit() ? LogicValue::One : LogicValue::Zero;
        }
    }

    const auto components = circuit_.components();
    for (const auto& component : components) {
        PowerOnContext ctx(*this, component->firstPin(), component->id());
        component->powerOn(ctx);
    }

    std::ranges::fill(driving_, Driving{0, 0});
    for (std::uint32_t p = 0; p < outputs_.size(); ++p) {
        if (IsDriver(circuit_.pinInfo(PinId{p}).kind)) {
            countDriving(PinId{p}, LogicValue::Z, outputs_[p].current);
        }
    }

    for (std::uint32_t n = 0; n < nets_.size(); ++n) {
        NetState& s = nets_[n];
        s.lastDriven = rng_.nextBit() ? LogicValue::One : LogicValue::Zero;
        if (s.networked) {
            external_[n] = {externalDrives(NetId{n}, circuit_.networkPin(NetId{n})), -1};
            if (const PinId second = circuit_.secondNetworkPin(NetId{n});
                second != Circuit::kNoPin) {
                external2_[n] = {externalDrives(NetId{n}, second), -1};
            }
        }
        const Resolution r = resolveNet(NetId{n});
        s.value = r.value;
        s.previous = r.value;
        s.changedAt = -1;  // nothing "rose" at time 0
        s.floating = r.floating;
        s.dirty = false;
        if (!r.floating) {
            s.lastDriven = r.value;
        }
    }

    for (const auto& component : components) {
        EvalContext ctx(*this, component->firstPin(), component->id());
        component->evaluate(ctx);
        ++stats_.evaluations;
    }
}

void Simulator::runUntil(Time end) {
    Check(end >= now_, "simulator: runUntil into the past");
    const auto components = circuit_.components();
    // The per-Pin, per-Net and per-Component arrays never resize during a run. Held in locals, so
    // that the byte stores below (a uint8_t or a LogicValue may alias anything) do not make the
    // compiler load every member again; the counters likewise, added to stats_ at the end.
    NetState* const nets = nets_.data();
    const NetId* const pinNet = pinNet_.data();
    OutputState* const outputs = outputs_.data();
    Driving* const driving = driving_.data();
    const FanOut* const fanOut = fanOut_.data();
    const std::uint32_t* const fanOutStart = fanOutStart_.data();
    std::uint8_t* const componentDirty = componentDirty_.data();
    const bool recording = !recorder_.nets().empty();  // only between runs does it change
    std::uint64_t applied = 0, stale = 0, wakes = 0, evaluations = 0;
    const auto markDirty = [&](ComponentId component) {
        std::uint8_t& dirty = componentDirty[ToIndex(component)];
        if (dirty == 0) {
            dirty = 1;
            dirtyComponents_.push_back(component);
        }
    };

    while (!queue_.empty() && queue_.nextTime() <= end) {
        now_ = queue_.nextTime();

        // Update phase: apply due output changes, then resolve their Nets.
        for (const EventKey key : queue_.takeNext()) {
            const std::uint32_t target = KeyTarget(key);
            const std::uint32_t stamp = KeyStamp(key);
            if (StampIsWake(stamp)) {
                markDirty(ComponentId{target});
                ++wakes;
                continue;
            }
            OutputState& o = outputs[target];
            if (StampGeneration(stamp) != o.generation) {
                ++stale;
                continue;
            }
            const LogicValue value = StampValue(stamp);
            const NetId net = pinNet[target];
            if ((o.current == LogicValue::Z) != (value == LogicValue::Z)) {  // countDriving
                Driving& d = driving[ToIndex(net)];
                if (value == LogicValue::Z) {
                    --d.count;
                    d.pinSum -= target;
                } else {
                    ++d.count;
                    d.pinSum += target;
                }
            }
            o.current = value;
            o.hasPending = false;
            ++applied;
            NetState& s = nets[ToIndex(net)];
            if (!s.dirty) {
                s.dirty = true;
                dirtyNets_.push_back(net);
            }
        }

        for (const NetId net : dirtyNets_) {
            NetState& s = nets[ToIndex(net)];
            if (s.networked) {
                updateExternal(net, circuit_.networkPin(net), external_[ToIndex(net)]);
                if (const PinId second = circuit_.secondNetworkPin(net);
                    second != Circuit::kNoPin) {
                    updateExternal(net, second, external2_[ToIndex(net)]);
                }
            }
            const Resolution r = resolveNet(net);
            s.dirty = false;
            const bool wasFloating = s.floating;
            s.floating = r.floating;
            if (!r.floating) {
                s.lastDriven = r.value;
            }
            if (r.value != s.value) {
                s.previous = s.value;
                s.value = r.value;
                s.changedAt = now_;
                if (recording && recorder_.isRecording(net)) {
                    recorder_.add(net, {now_, r.value, r.floating});
                }
                const std::uint8_t change =
                    s.previous == LogicValue::Zero && s.value == LogicValue::One   ? kWakeOnRise
                    : s.previous == LogicValue::One && s.value == LogicValue::Zero ? kWakeOnFall
                                                                                   : kWakeOnOther;
                const std::uint32_t n = ToIndex(net);
                const std::uint32_t last = fanOutStart[n + 1];
                for (std::uint32_t e = fanOutStart[n]; e < last; ++e) {
                    const FanOut& f = fanOut[e];
                    if ((f.edges & change) && f.pins != 0) {
                        markDirty(f.component);
                    }
                }
            } else if (recording && r.floating != wasFloating && recorder_.isRecording(net)) {
                recorder_.add(net, {now_, r.value, r.floating});
            }
        }
        dirtyNets_.clear();

        // Evaluate phase: evaluations only schedule, so the dirty list is stable.
        for (const ComponentId id : dirtyComponents_) {
            componentDirty[ToIndex(id)] = 0;
            Component& component = *components[ToIndex(id)];
            EvalContext ctx(*this, component.firstPin(), id);
            component.evaluate(ctx);
            ++evaluations;
        }
        dirtyComponents_.clear();
        changedNetworkPins_.clear();
    }
    stats_.appliedEvents += applied;
    stats_.staleEvents += stale;
    stats_.wakes += wakes;
    stats_.evaluations += evaluations;
    now_ = end;
}

}  // namespace bw
