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
};

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
            pins_.push_back({kUnconnected, base.id_, spec.kind, spec.family});
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
    std::span<const ComponentId> readers(NetId net) const {
        const std::uint32_t i = ToIndex(net);
        return std::span(readers_).subspan(readerStart_[i], readerStart_[i + 1] - readerStart_[i]);
    }
    Drive tieOf(NetId net) const { return netTie_[ToIndex(net)]; }
    bool hasTtlInput(NetId net) const { return netHasTtlInput_[ToIndex(net)] != 0; }

private:
    static constexpr NetId kUnconnected{UINT32_MAX};

    void checkNet(NetId net) const;

    bool finalized_ = false;
    std::vector<std::unique_ptr<Component>> components_;
    std::vector<PinInfo> pins_;
    std::vector<std::string> netNames_;
    std::vector<Drive> netTie_;  // strongest tie per Net, None if untied
    std::vector<std::uint8_t> netHasTtlInput_;
    // CSR: drivers (output PinIds) and readers (unique ComponentIds with an input on the Net)
    std::vector<std::uint32_t> driverStart_, readerStart_;  // size netCount + 1
    std::vector<PinId> drivers_;
    std::vector<ComponentId> readers_;
};

}  // namespace bw
