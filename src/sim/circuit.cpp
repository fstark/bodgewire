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
            const std::size_t local = static_cast<std::size_t>(&info - pins_.data()) -
                                      ToIndex(owner.firstPin());
            info.net = addNet(std::string(owner.name()) + "." + owner.pins()[local].name);
        }
    }

    const std::size_t netCount = netNames_.size();
    std::vector<std::vector<PinId>> drivers(netCount);
    std::vector<std::vector<ComponentId>> readers(netCount);
    netHasTtlInput_.assign(netCount, 0);
    for (std::uint32_t p = 0; p < pins_.size(); ++p) {
        const PinInfo& info = pins_[p];
        const std::uint32_t n = ToIndex(info.net);
        if (info.kind != PinKind::Input) {
            drivers[n].push_back(PinId{p});
            continue;
        }
        if (info.family == Family::Ttl) {
            netHasTtlInput_[n] = 1;
        }
        // Pins of one Component are contiguous, so checking the last reader deduplicates.
        if (readers[n].empty() || readers[n].back() != info.owner) {
            readers[n].push_back(info.owner);
        }
    }

    driverStart_.assign(netCount + 1, 0);
    readerStart_.assign(netCount + 1, 0);
    drivers_.clear();
    readers_.clear();
    for (std::size_t n = 0; n < netCount; ++n) {
        drivers_.insert(drivers_.end(), drivers[n].begin(), drivers[n].end());
        readers_.insert(readers_.end(), readers[n].begin(), readers[n].end());
        driverStart_[n + 1] = static_cast<std::uint32_t>(drivers_.size());
        readerStart_[n + 1] = static_cast<std::uint32_t>(readers_.size());
    }
    finalized_ = true;
}

}  // namespace bw
