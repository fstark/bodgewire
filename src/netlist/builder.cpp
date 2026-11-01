#include "netlist/builder.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <utility>

#include "parts/button.hpp"
#include "parts/catalogue.hpp"
#include "parts/connector.hpp"
#include "sim/check.hpp"

namespace bw {

namespace {

struct PlacedPart {
    const PartDecl* decl;
    const PartInfo* info;  // nullptr: unknown part
    Component* component;  // nullptr: resistor, or creation failed
};

struct NetOfPin {
    NetId net;
    std::string_view name;
};

bool AllDigits(std::string_view text) {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

bool IsResistor(const PlacedPart& part) {
    return part.info != nullptr && part.info->kind == PartKind::Resistor;
}

void SortByLine(std::vector<Diagnostic>& diagnostics) {
    std::stable_sort(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic& a, const Diagnostic& b) { return a.line < b.line; });
}

class Builder {
public:
    explicit Builder(const Netlist& netlist, std::span<const StuckPin> stuck,
                     std::span<const std::string> unpowered)
        : netlist_(netlist), stuck_(stuck), unpowered_(unpowered),
          loaded_(std::make_unique<LoadedCircuit>()) {}

    LoadResult build() {
        Circuit& circuit = loaded_->circuit;
        circuit.tie(netNamed("GND"), LogicValue::Zero, DriveStrength::Supply);
        circuit.tie(netNamed("VCC"), LogicValue::One, DriveStrength::Supply);
        placeParts();
        connectNets();
        addImplicitPower();
        tieResistors();
        wireButtons();
        if (!errors_.empty()) {
            SortByLine(errors_);
            return std::unexpected(std::move(errors_));
        }
        for (const StuckPin& s : stuck_) {
            const auto it = loaded_->components.find(s.reference);
            Check(it != loaded_->components.end() && s.pin < it->second->pins().size() &&
                      IsDriver(it->second->pins()[s.pin].kind) &&
                      (s.value == LogicValue::Zero || s.value == LogicValue::One),
                  "builder: stuck pin must be an output of a placed Component");
        }
        for (const auto& [pin, net] : pinNet_) {
            const PlacedPart& part = placed_.find(pin.first)->second;
            if (isUnpowered(pin.first)) {
                // no Driver, no Load: the chip's Pins get private Nets at finalize()
            } else if (const StuckPin* s = stuckAt(pin.first, pin.second)) {
                circuit.tie(net.net, s->value, DriveStrength::Strong);
            } else if (notConnected_.contains(pin)) {
                // a Button's rail end: the Button drives the signal end only
            } else if (part.component != nullptr) {
                circuit.connect(*part.component, pin.second, net.net);
            }
            loaded_->connections.push_back({pin.first, pin.second, std::string(net.name)});
        }
        for (const PartDecl& decl : netlist_.parts) {
            const PlacedPart& part = placed_.find(decl.reference)->second;
            std::string value = decl.part;
            if (IsResistor(part)) {
                for (const PartParam& param : decl.params) {
                    value = param.value;  // only "value" is accepted
                }
            }
            loaded_->parts.push_back({decl.reference, part.info, std::move(value)});
        }
        warnUnconnectedInputs();
        circuit.finalize();
        return std::move(loaded_);
    }

private:
    const StuckPin* stuckAt(std::string_view reference, PinIndex pin) const {
        for (const StuckPin& s : stuck_) {
            if (s.reference == reference && s.pin == pin) {
                return &s;
            }
        }
        return nullptr;
    }

    bool isUnpowered(std::string_view reference) const {
        return std::ranges::find(unpowered_, reference) != unpowered_.end();
    }

    void error(std::uint32_t line, std::string message) {
        errors_.push_back({netlist_.file, line, Severity::Error, std::move(message)});
    }

    NetId netNamed(std::string_view name) {
        if (const auto it = loaded_->nets.find(name); it != loaded_->nets.end()) {
            return it->second;
        }
        const NetId net = loaded_->circuit.addNet(std::string(name));
        loaded_->nets.emplace(std::string(name), net);
        return net;
    }

    void placeParts() {
        for (const PartDecl& decl : netlist_.parts) {
            if (const auto it = placed_.find(decl.reference); it != placed_.end()) {
                error(decl.line, decl.reference + " already placed at line " +
                                     std::to_string(it->second.decl->line));
                continue;
            }
            PlacedPart& part = placed_[decl.reference];
            part = {&decl, FindPart(decl.part), nullptr};
            if (part.info == nullptr) {
                error(decl.line, "unknown part '" + decl.part + "'");
                continue;
            }
            if (part.info->kind == PartKind::Resistor) {
                for (const PartParam& param : decl.params) {
                    if (param.name != "value") {
                        error(decl.line,
                              decl.reference + ": unknown parameter '" + param.name + "'");
                        part.info = nullptr;
                        break;
                    }
                }
                continue;
            }
            auto created = part.info->create(loaded_->circuit, decl.reference, decl.params);
            if (!created) {
                error(decl.line, decl.reference + ": " + created.error());
                continue;
            }
            part.component = *created;
            loaded_->components.emplace(decl.reference, part.component);
        }
    }

    std::optional<PinIndex> resolvePin(const PlacedPart& part, const PinRef& ref,
                                       std::uint32_t line) {
        if (IsResistor(part)) {
            if (ref.pin == "1" || ref.pin == "2") {
                return static_cast<PinIndex>(ref.pin[0] - '1');
            }
            error(line, ref.reference + " (" + part.decl->part + ") has no pin " + ref.pin);
            return std::nullopt;
        }
        const auto pins = part.component->pins();
        if (AllDigits(ref.pin)) {
            if (ref.pin.size() <= 3) {
                const std::size_t number = std::stoul(ref.pin);
                if (number >= 1 && number <= pins.size()) {
                    return static_cast<PinIndex>(number - 1);
                }
            }
        } else {
            for (PinIndex i = 0; i < pins.size(); ++i) {
                if (pins[i].name == ref.pin) {
                    return i;
                }
            }
        }
        error(line, ref.reference + " (" + part.decl->part + ") has no pin " + ref.pin);
        return std::nullopt;
    }

    void connectNets() {
        for (const NetDecl& decl : netlist_.nets) {
            const NetId net = netNamed(decl.name);
            for (const PinRef& ref : decl.pins) {
                const auto it = placed_.find(ref.reference);
                if (it == placed_.end()) {
                    error(decl.line, "unknown reference '" + ref.reference + "'");
                    continue;
                }
                const PlacedPart& part = it->second;
                const bool resistor = IsResistor(part);
                if (!resistor && part.component == nullptr) {
                    continue;  // already reported at its part line
                }
                const auto index = resolvePin(part, ref, decl.line);
                if (!index) {
                    continue;
                }
                std::string where = ref.reference + "." + ref.pin;
                if (!resistor) {
                    const PinSpec& spec = part.component->pins()[*index];
                    where += " (" + spec.name + ")";
                    if (spec.kind == PinKind::NoConnect) {
                        error(decl.line, where + " is not connected inside " + part.decl->part);
                        continue;
                    }
                    if (spec.kind == PinKind::Power && decl.name != spec.name) {
                        error(decl.line, where + " can only be on net " + spec.name);
                        continue;
                    }
                }
                const auto [existing, inserted] =
                    pinNet_.try_emplace({ref.reference, *index}, NetOfPin{net, decl.name});
                if (!inserted && existing->second.name != decl.name) {
                    error(decl.line, where + " is on nets " + std::string(existing->second.name) +
                                         " and " + decl.name);
                }
            }
        }
    }

    void addImplicitPower() {
        for (const auto& [reference, part] : placed_) {
            if (part.component == nullptr) {
                continue;
            }
            const auto pins = part.component->pins();
            for (PinIndex i = 0; i < pins.size(); ++i) {
                if (pins[i].kind == PinKind::Power) {
                    const std::string_view rail = loaded_->nets.find(pins[i].name)->first;
                    pinNet_.try_emplace({reference, i}, NetOfPin{netNamed(rail), rail});
                }
            }
        }
    }

    // Pull-up / pull-down only: one pin on a rail, the other on a signal Net.
    void tieResistors() {
        for (const auto& [reference, part] : placed_) {
            if (!IsResistor(part)) {
                continue;
            }
            const auto a = pinNet_.find({reference, 0});
            const auto b = pinNet_.find({reference, 1});
            if (a == pinNet_.end() || b == pinNet_.end() ||
                IsRail(a->second.name) == IsRail(b->second.name)) {
                error(part.decl->line, reference +
                                           ": only pull-up/pull-down resistors are supported (one "
                                           "pin on GND or VCC, the other on a signal net)");
                continue;
            }
            const NetOfPin& rail = IsRail(a->second.name) ? a->second : b->second;
            const NetOfPin& signal = IsRail(a->second.name) ? b->second : a->second;
            loaded_->circuit.tie(signal.net,
                                 rail.name == "VCC" ? LogicValue::One : LogicValue::Zero,
                                 DriveStrength::Weak);
        }
    }

    // Same rule as resistors: one pin on a rail, the other on a signal Net.
    void wireButtons() {
        for (const auto& [reference, part] : placed_) {
            if (part.info == nullptr || part.info->kind != PartKind::Button ||
                part.component == nullptr) {
                continue;
            }
            const auto a = pinNet_.find({reference, 0});
            const auto b = pinNet_.find({reference, 1});
            if (a == pinNet_.end() || b == pinNet_.end() ||
                IsRail(a->second.name) == IsRail(b->second.name)) {
                error(part.decl->line, reference +
                                           ": a button needs one pin on GND or VCC and the other "
                                           "on a signal net");
                continue;
            }
            const PinIndex rail = IsRail(a->second.name) ? 0 : 1;
            const std::string_view railNet = (rail == 0 ? a : b)->second.name;
            static_cast<Button&>(*part.component)
                .attach(1 - rail, railNet == "VCC" ? LogicValue::One : LogicValue::Zero);
            notConnected_.insert({reference, rail});
        }
    }

    void warnUnconnectedInputs() {
        for (const auto& [reference, part] : placed_) {
            if (part.component == nullptr) {
                continue;
            }
            const auto pins = part.component->pins();
            for (PinIndex i = 0; i < pins.size(); ++i) {
                if (pins[i].kind == PinKind::Input && !pinNet_.contains({reference, i})) {
                    loaded_->warnings.push_back({netlist_.file, part.decl->line, Severity::Warning,
                                                 reference + "." + std::to_string(i + 1) + " (" +
                                                     pins[i].name +
                                                     ") is unconnected: floats high"});
                }
            }
        }
        SortByLine(loaded_->warnings);
    }

    const Netlist& netlist_;
    std::span<const StuckPin> stuck_;
    std::span<const std::string> unpowered_;
    std::unique_ptr<LoadedCircuit> loaded_;
    std::map<std::string, PlacedPart, std::less<>> placed_;  // by reference
    std::map<std::pair<std::string, PinIndex>, NetOfPin> pinNet_;
    std::set<std::pair<std::string, PinIndex>> notConnected_;
    std::vector<Diagnostic> errors_;
};

}  // namespace

bool IsRail(std::string_view net) { return net == "GND" || net == "VCC"; }

LoadResult BuildCircuit(const Netlist& netlist, std::span<const StuckPin> stuck,
                        std::span<const std::string> unpowered) {
    return Builder(netlist, stuck, unpowered).build();
}

LoadResult LoadNetlist(std::string_view text, std::string file) {
    const auto netlist = ParseNetlist(text, std::move(file));
    if (!netlist) {
        return std::unexpected(netlist.error());
    }
    return BuildCircuit(*netlist);
}

LoadResult LoadNetlistFile(const std::filesystem::path& path) {
    std::string file = path.filename().string();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::unexpected(
            std::vector<Diagnostic>{{std::move(file), 0, Severity::Error, "cannot read file"}});
    }
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    return LoadNetlist(text, std::move(file));
}

std::optional<PinIndex> FindPin(const Component& component, std::string_view pin) {
    const auto pins = component.pins();
    if (AllDigits(pin)) {
        if (pin.size() <= 3) {
            const std::size_t number = std::stoul(std::string(pin));
            if (number >= 1 && number <= pins.size()) {
                return static_cast<PinIndex>(number - 1);
            }
        }
        return std::nullopt;
    }
    for (PinIndex i = 0; i < pins.size(); ++i) {
        if (pins[i].name == pin) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<NetId> PinNet(const LoadedCircuit& loaded, std::string_view reference,
                            std::string_view pin) {
    const auto component = loaded.components.find(reference);
    if (component == loaded.components.end()) {
        return std::nullopt;
    }
    const auto index = FindPin(*component->second, pin);
    if (!index) {
        return std::nullopt;
    }
    for (const Connection& c : loaded.connections) {
        if (c.reference == reference && c.pin == *index) {
            return loaded.nets.at(c.net);
        }
    }
    return loaded.circuit.netOf(PinId{ToIndex(component->second->firstPin()) + *index});
}

Button* ButtonOf(const LoadedCircuit& loaded, std::string_view reference) {
    const auto part = std::ranges::find(loaded.parts, reference, &LoadedPart::reference);
    if (part == loaded.parts.end() || part->info->kind != PartKind::Button) {
        return nullptr;
    }
    const auto component = loaded.components.find(reference);
    return component == loaded.components.end() ? nullptr : static_cast<Button*>(component->second);
}

std::optional<NetId> ConnectorNet(const LoadedCircuit& loaded, std::string_view part,
                                  std::string_view pin) {
    for (const LoadedPart& p : loaded.parts) {
        if (p.info->name == part) {
            return PinNet(loaded, p.reference, pin);
        }
    }
    return std::nullopt;
}

std::optional<double> RailVolts(const LoadedCircuit& loaded, NetId net) {
    for (const LoadedPart& part : loaded.parts) {
        if (part.info->kind != PartKind::Connector) {
            continue;
        }
        for (const PinSpec& spec : ConnectorPins(part.info->name)) {
            const auto volts = ConnectorVolts(part.info->name, spec.name);
            if (volts && PinNet(loaded, part.reference, spec.name) == net) {
                return volts;
            }
        }
    }
    return std::nullopt;
}

}  // namespace bw
