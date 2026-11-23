#pragma once

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sim/ids.hpp"
#include "sim/logic.hpp"
#include "sim/time.hpp"

namespace bw {

class Simulator;

struct PinSpec {
    std::string name;  // datasheet name, NAMING.md "Pin names"
    PinKind kind;
    Family family;
    bool risingEdgeOnly = false;  // Input: other changes never alter evaluate()'s result
    // Input read only when another Pin acts (a flip-flop's D): its changes never alter
    // evaluate()'s result, so they do not wake the Component.
    bool sampled = false;
    // Input whose changes other than 1 -> 0 never alter evaluate()'s result (a clock read only
    // through fell()). Last, after `sampled`: Parts build PinSpecs positionally.
    bool fallingEdgeOnly = false;
};

// Pins the datasheet names as one value by letters (QA-QD), not digits: the Logic Analyzer's
// Shift-click groups them (pins named with digits, A0-A15, are grouped by their names).
struct PinGroup {
    std::string name;            // "QA-QD"
    std::vector<PinIndex> pins;  // bit 0 first
};

// Passed to Component::evaluate(); hot members are inline in simulator.hpp, include it to use them.
class EvalContext {
public:
    Time now() const;
    inline LogicValue read(PinIndex pin) const;        // Net value now
    inline LogicValue readBefore(PinIndex pin) const;  // Net value before this timestamp (S3)
    // The Net's value now in `value`; true if it changed at this timestamp (P5z2: one read of
    // the Net for both).
    inline bool readChanged(PinIndex pin, LogicValue& value) const;
    inline bool rose(PinIndex pin) const;  // Net went 0 → 1 at this timestamp
    inline bool fell(PinIndex pin) const;  // Net went 1 → 0 at this timestamp
    bool readBit(PinIndex pin);            // X → seeded random bit (MOS inputs, S4)
    bool readBitBefore(PinIndex pin);      // readBefore, X → seeded random bit
    inline void drive(PinIndex pin, LogicValue value, Time delay);  // inertial, delay ≥ 1 ps
    // While a Pin is quiet, its Net's changes do not wake this Component; what the Pin reads
    // and drives is unchanged. Every Pin is loud at power-on (P5z: a deselected memory).
    inline void quiet(PinIndex pin, bool on);
    inline Time lastChange(PinIndex pin) const;  // when the Net's value last changed; -1: never
    void wakeAfter(Time delay);                  // evaluate this Component again at now + delay
    void wake(ComponentId other, Time delay);    // evaluate `other` at now + delay (≥ 1 ps)
    // Switched and Joined Pins only (P5i, P5u): the drives on the Pin's Net but its own, and
    // whether they changed at this timestamp.
    inline const Resolver& external(PinIndex pin) const;
    inline bool externalChanged(PinIndex pin) const;

private:
    friend class Simulator;
    EvalContext(Simulator& sim, PinId firstPin, ComponentId component)
        : sim_(sim), firstPin_(firstPin), component_(component) {}
    PinId pinId(PinIndex pin) const { return PinId{ToIndex(firstPin_) + pin}; }
    void driveChanged(PinId id, LogicValue value, Time delay);
    Simulator& sim_;
    PinId firstPin_;
    ComponentId component_;
};

// Passed to Component::powerOn(); members defined in simulator.cpp.
class PowerOnContext {
public:
    bool randomBit();
    void setOutput(PinIndex pin, LogicValue value);  // initial output, no event

private:
    friend class Simulator;
    PowerOnContext(Simulator& sim, PinId firstPin, ComponentId component)
        : sim_(sim), firstPin_(firstPin), component_(component) {}
    Simulator& sim_;
    PinId firstPin_;
    [[maybe_unused]] ComponentId component_;
};

class Component {
public:
    virtual ~Component() = default;
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    std::string_view name() const { return name_; }
    std::span<const PinSpec> pins() const { return pins_; }
    ComponentId id() const { return id_; }              // valid once added to a Circuit
    PinId firstPin() const { return firstPin_; }        // global PinId of local Pin 0
    PinIndex pinIndex(std::string_view pinName) const;  // Check()s the name
    std::span<const PinGroup> pinGroups() const { return pinGroups_; }

    // Outputs already hold random 0/1, bidirectional Pins Z; override to seed state / make outputs
    // consistent.
    virtual void powerOn(PowerOnContext& ctx);
    virtual void evaluate(EvalContext& ctx) = 0;

protected:
    Component(std::string name, std::vector<PinSpec> pins);
    // Adds a PinGroup over these Pins, bit 0 first; Check()s the names.
    void declarePinGroup(std::string name, std::initializer_list<std::string_view> pins);

private:
    friend class Circuit;
    std::string name_;
    std::vector<PinSpec> pins_;
    std::vector<PinGroup> pinGroups_;
    ComponentId id_{};
    PinId firstPin_{};
};

}  // namespace bw
