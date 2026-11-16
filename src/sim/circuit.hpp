#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sim/check.hpp"
#include "sim/component.hpp"
#include "sim/ids.hpp"
#include "sim/logic.hpp"

namespace bw {

struct PinInfo {
    NetId net;
    ComponentId owner;
    PinKind kind;
    Family family;
    bool risingEdgeOnly;
    bool sampled;
    bool fallingEdgeOnly;
};

// The changes of a Net that wake a reader: one bit per kind of change.
inline constexpr std::uint8_t kWakeOnRise = 1;   // 0 -> 1
inline constexpr std::uint8_t kWakeOnFall = 2;   // 1 -> 0
inline constexpr std::uint8_t kWakeOnOther = 4;  // any other change of value (to or from X, Z)
inline constexpr std::uint8_t kWakeOnAny = kWakeOnRise | kWakeOnFall | kWakeOnOther;

// One per (Net, reading Component): all Input Pins of that Component on that Net that can wake
// it (sampled Pins are left out).
struct FanOut {
    ComponentId component;
    std::uint8_t edges;  // the kWakeOn* bits of those Pins, ORed
};
static_assert(sizeof(FanOut) == 8);

// Fixed topology: built, then finalized; flat arrays indexed by ToIndex(id).
class Circuit {
public:
    NetId addNet(std::string name);

    template <std::derived_from<Component> T, class... Args>
    T& add(Args&&... args) {
        Check(!finalized_, "circuit: add after finalize");
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T& result = *component;
        Component& base = result;
        base.id_ = ComponentId{static_cast<std::uint32_t>(components_.size())};
        base.firstPin_ = PinId{static_cast<std::uint32_t>(pins_.size())};
        for (const PinSpec& spec : base.pins()) {
            Check(!(spec.risingEdgeOnly && spec.fallingEdgeOnly),
                  "circuit: a pin both rising- and falling-edge-only");
            pins_.push_back({kUnconnected, base.id_, spec.kind, spec.family, spec.risingEdgeOnly,
                             spec.sampled, spec.fallingEdgeOnly});
        }
        components_.push_back(std::move(component));
        return result;
    }

    void connect(const Component& component, PinIndex pin, NetId net);
    void connect(const Component& component, std::string_view pin, NetId net);
    void tie(NetId net, LogicValue value, DriveStrength strength);  // GND/VCC or pull resistor
    void finalize();  // unconnected Pins get private Nets; builds lookup tables; no more edits

    std::size_t netCount() const { return netNames_.size(); }
    std::string_view netName(NetId net) const { return netNames_[ToIndex(net)]; }
    NetId netOf(PinId pin) const { return pins_[ToIndex(pin)].net; }

    bool isFinalized() const { return finalized_; }
    std::span<const std::unique_ptr<Component>> components() const { return components_; }
    std::size_t pinCount() const { return pins_.size(); }
    const PinInfo& pinInfo(PinId pin) const { return pins_[ToIndex(pin)]; }
    std::span<const PinId> drivers(NetId net) const {
        const std::uint32_t i = ToIndex(net);
        return std::span(drivers_).subspan(driverStart_[i], driverStart_[i + 1] - driverStart_[i]);
    }
    std::span<const FanOut> fanOut(NetId net) const {
        const std::uint32_t i = ToIndex(net);
        return std::span(fanOut_).subspan(fanOutStart_[i], fanOutStart_[i + 1] - fanOutStart_[i]);
    }
    Drive tieOf(NetId net) const { return netTie_[ToIndex(net)]; }
    bool hasTtlInput(NetId net) const { return netHasTtlInput_[ToIndex(net)] != 0; }
    // The Net's only Driver if it is a totem-pole Output and the Net is untied, else kNoPin.
    PinId singleDriver(NetId net) const { return netSingleDriver_[ToIndex(net)]; }
    // The Net's network Pins (P5i, P5u): at most one Switched and one Joined; the first found,
    // else kNoPin, and the second, else kNoPin.
    PinId networkPin(NetId net) const { return netNetworkPin_[ToIndex(net)]; }
    PinId secondNetworkPin(NetId net) const { return netNetworkPin2_[ToIndex(net)]; }

    static constexpr PinId kNoPin{UINT32_MAX};

private:
    static constexpr NetId kUnconnected{UINT32_MAX};

    void checkNet(NetId net) const;
    void buildTables();  // drivers_, fanOut_, netHasTtlInput_, netSingleDriver_ from pins_

    bool finalized_ = false;
    std::vector<std::unique_ptr<Component>> components_;
    std::vector<PinInfo> pins_;
    std::vector<std::string> netNames_;
    std::vector<Drive> netTie_;  // strongest tie per Net, None if untied
    std::vector<std::uint8_t> netHasTtlInput_;
    std::vector<PinId> netSingleDriver_;
    std::vector<PinId> netNetworkPin_;
    std::vector<PinId> netNetworkPin2_;
    // CSR: drivers (output PinIds) and fan-out (one entry per Component with an input on the Net)
    std::vector<std::uint32_t> driverStart_, fanOutStart_;  // size netCount + 1
    std::vector<PinId> drivers_;
    std::vector<FanOut> fanOut_;
};

}  // namespace bw
