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
    nets_.resize(circuit.netCount(),
                 NetState{-1, LogicValue::Zero, LogicValue::Zero, LogicValue::Zero, false, false});
    outputs_.resize(circuit.pinCount(), OutputState{0, LogicValue::Zero, LogicValue::Zero, false});
    componentDirty_.resize(circuit.components().size(), 0);
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

Resolution Simulator::resolveNet(NetId net) const {
    const PinId single = circuit_.singleDriver(net);
    if (single != Circuit::kNoPin) {
        const LogicValue value = outputs_[ToIndex(single)].current;
        if (value != LogicValue::Z) {
            return {value, false};  // what Resolver gives for one Strong drive and no tie
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

void Simulator::record(NetId net) {
    Check(ToIndex(net) < nets_.size(), "simulator: unknown net");
    recorder_.start(net, now_, value(net));
}

void Simulator::powerOn(std::uint64_t seed) {
    queue_.clear();
    now_ = 0;
    rng_ = Rng(seed);
    recorder_.clear();
    stats_ = {};
    dirtyNets_.clear();
    dirtyComponents_.clear();
    std::fill(componentDirty_.begin(), componentDirty_.end(), 0);

    for (std::uint32_t p = 0; p < outputs_.size(); ++p) {
        OutputState& o = outputs_[p];
        o = {0, LogicValue::Zero, LogicValue::Zero, false};
        if (IsDriver(circuit_.pinInfo(PinId{p}).kind)) {
            o.current = rng_.nextBit() ? LogicValue::One : LogicValue::Zero;
        }
    }

    const auto components = circuit_.components();
    for (const auto& component : components) {
        PowerOnContext ctx(*this, component->firstPin(), component->id());
        component->powerOn(ctx);
    }

    for (std::uint32_t n = 0; n < nets_.size(); ++n) {
        NetState& s = nets_[n];
        s.lastDriven = rng_.nextBit() ? LogicValue::One : LogicValue::Zero;
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
    while (!queue_.empty() && queue_.top().time <= end) {
        now_ = queue_.top().time;

        // Update phase: apply due output changes, then resolve their Nets.
        while (!queue_.empty() && queue_.top().time == now_) {
            const Event event = queue_.top();
            queue_.pop();
            if (StampIsWake(event.stamp)) {
                markComponentDirty(ComponentId{event.target});
                ++stats_.wakes;
                continue;
            }
            OutputState& o = outputs_[event.target];
            if (StampGeneration(event.stamp) != o.generation) {
                ++stats_.staleEvents;
                continue;
            }
            o.current = StampValue(event.stamp);
            o.hasPending = false;
            ++stats_.appliedEvents;
            markNetDirty(circuit_.netOf(PinId{event.target}));
        }

        for (const NetId net : dirtyNets_) {
            const Resolution r = resolveNet(net);
            NetState& s = nets_[ToIndex(net)];
            s.dirty = false;
            s.floating = r.floating;
            if (!r.floating) {
                s.lastDriven = r.value;
            }
            if (r.value != s.value) {
                s.previous = s.value;
                s.value = r.value;
                s.changedAt = now_;
                if (recorder_.isRecording(net)) {
                    recorder_.add(net, now_, r.value);
                }
                const bool rose = s.previous == LogicValue::Zero && s.value == LogicValue::One;
                for (const FanOut& f : circuit_.fanOut(net)) {
                    if (rose || !f.risingEdgeOnly) {
                        markComponentDirty(f.component);
                    }
                }
            }
        }
        dirtyNets_.clear();

        // Evaluate phase: evaluations only schedule, so the dirty list is stable.
        for (const ComponentId id : dirtyComponents_) {
            componentDirty_[ToIndex(id)] = 0;
            Component& component = *components[ToIndex(id)];
            EvalContext ctx(*this, component.firstPin(), id);
            component.evaluate(ctx);
            ++stats_.evaluations;
        }
        dirtyComponents_.clear();
    }
    now_ = end;
}

}  // namespace bw
