#pragma once

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
};

// Passed to Component::evaluate(); hot members are inline in simulator.hpp, include it to use them.
class EvalContext {
public:
    Time now() const;
    inline LogicValue read(PinIndex pin) const;        // Net value now
    inline LogicValue readBefore(PinIndex pin) const;  // Net value before this timestamp (S3)
    inline bool rose(PinIndex pin) const;              // Net went 0 → 1 at this timestamp
    inline bool fell(PinIndex pin) const;              // Net went 1 → 0 at this timestamp
    bool readBit(PinIndex pin);                        // X → seeded random bit (MOS inputs, S4)
    bool readBitBefore(PinIndex pin);                  // readBefore, X → seeded random bit
    inline void drive(PinIndex pin, LogicValue value, Time delay);  // inertial, delay ≥ 1 ps
    void wakeAfter(Time delay);  // evaluate this Component again at now + delay

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

    // Outputs already hold random 0/1, bidirectional Pins Z; override to seed state / make outputs
    // consistent.
    virtual void powerOn(PowerOnContext& ctx);
    virtual void evaluate(EvalContext& ctx) = 0;

protected:
    Component(std::string name, std::vector<PinSpec> pins);

private:
    friend class Circuit;
    std::string name_;
    std::vector<PinSpec> pins_;
    ComponentId id_{};
    PinId firstPin_{};
};

}  // namespace bw
