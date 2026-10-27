#include "sim/circuit.hpp"

namespace bw {

NetId Circuit::addNet(std::string name) {
    Check(!finalized_, "circuit: addNet after finalize");
    const NetId net{static_cast<std::uint32_t>(netNames_.size())};
    netNames_.push_back(std::move(name));
    netTie_.push_back({LogicValue::Z, DriveStrength::None, Family::Ttl});
    return net;
}

void Circuit::checkNet(NetId net) const {
    Check(ToIndex(net) < netNames_.size(), "circuit: unknown net");
}

void Circuit::connect(const Component& component, PinIndex pin, NetId net) {
    Check(!finalized_, "circuit: connect after finalize");
    const std::uint32_t id = ToIndex(component.id());
    Check(id < components_.size() && components_[id].get() == &component,
          "circuit: component not in this circuit");
    Check(pin < component.pins().size(), "circuit: pin index out of range");
    checkNet(net);
    PinInfo& info = pins_[ToIndex(component.firstPin()) + pin];
    Check(info.net == kUnconnected, std::string(component.name()) + ": pin " +
                                        component.pins()[pin].name + " connected twice");
    info.net = net;
}

void Circuit::connect(const Component& component, std::string_view pin, NetId net) {
    connect(component, component.pinIndex(pin), net);
}

void Circuit::tie(NetId net, LogicValue value, DriveStrength strength) {
    Check(!finalized_, "circuit: tie after finalize");
    checkNet(net);
    Check(strength != DriveStrength::None, "circuit: tie needs a strength");
    Check(value == LogicValue::Zero || value == LogicValue::One, "circuit: tie value must be 0/1");
    Drive& existing = netTie_[ToIndex(net)];
    if (strength > existing.strength) {
        existing = {value, strength, Family::Ttl};
    } else if (strength == existing.strength && value != existing.value) {
        existing.value = LogicValue::X;
    }
}

void Circuit::finalize() {
    Check(!finalized_, "circuit: finalize twice");
    for (PinInfo& info : pins_) {
        if (info.net == kUnconnected) {
            const Component& owner = *components_[ToIndex(info.owner)];
            const std::size_t local =
                static_cast<std::size_t>(&info - pins_.data()) - ToIndex(owner.firstPin());
            info.net = addNet(std::string(owner.name()) + "." + owner.pins()[local].name);
        }
    }
    buildTables();
    finalized_ = true;
}

void Circuit::buildTables() {
    const std::size_t netCount = netNames_.size();
    std::vector<std::vector<PinId>> drivers(netCount);
    std::vector<std::vector<FanOut>> fanOut(netCount);
    netHasTtlInput_.assign(netCount, 0);
    for (std::uint32_t p = 0; p < pins_.size(); ++p) {
        const PinInfo& info = pins_[p];
        const std::uint32_t n = ToIndex(info.net);
        if (IsDriver(info.kind)) {
            drivers[n].push_back(PinId{p});
            continue;
        }
        if (info.kind != PinKind::Input) {
            continue;
        }
        if (info.family == Family::Ttl) {
            netHasTtlInput_[n] = 1;
        }
        // Pins of one Component are contiguous, so checking the last entry merges them.
        if (fanOut[n].empty() || fanOut[n].back().component != info.owner) {
            fanOut[n].push_back({info.owner, info.risingEdgeOnly});
        } else {
            fanOut[n].back().risingEdgeOnly &= info.risingEdgeOnly;
        }
    }

    driverStart_.assign(netCount + 1, 0);
    fanOutStart_.assign(netCount + 1, 0);
    drivers_.clear();
    fanOut_.clear();
    netSingleDriver_.assign(netCount, kNoPin);
    for (std::size_t n = 0; n < netCount; ++n) {
        if (drivers[n].size() == 1 && pins_[ToIndex(drivers[n][0])].kind == PinKind::Output &&
            netTie_[n].strength == DriveStrength::None) {
            netSingleDriver_[n] = drivers[n][0];
        }
        drivers_.insert(drivers_.end(), drivers[n].begin(), drivers[n].end());
        fanOut_.insert(fanOut_.end(), fanOut[n].begin(), fanOut[n].end());
        driverStart_[n + 1] = static_cast<std::uint32_t>(drivers_.size());
        fanOutStart_[n + 1] = static_cast<std::uint32_t>(fanOut_.size());
    }
}

}  // namespace bw
