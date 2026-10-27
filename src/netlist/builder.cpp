#include "netlist/builder.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
#include <utility>

#include "parts/catalogue.hpp"

namespace bw {

namespace {

struct PlacedPart {
    const PartDecl* decl;
    const PartInfo* info;  // nullptr: unknown part
    Component* component;  // nullptr: resistor, or creation failed
};

struct PinNet {
    NetId net;
    std::string_view name;
};

bool AllDigits(std::string_view text) {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

bool IsRail(std::string_view net) { return net == "GND" || net == "VCC"; }

bool IsResistor(const PlacedPart& part) {
    return part.info != nullptr && part.info->kind == PartKind::Resistor;
}

void SortByLine(std::vector<Diagnostic>& diagnostics) {
    std::stable_sort(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic& a, const Diagnostic& b) { return a.line < b.line; });
}

class Builder {
public:
    explicit Builder(const Netlist& netlist)
        : netlist_(netlist), loaded_(std::make_unique<LoadedCircuit>()) {}

    LoadResult build() {
        Circuit& circuit = loaded_->circuit;
        circuit.tie(netNamed("GND"), LogicValue::Zero, DriveStrength::Supply);
        circuit.tie(netNamed("VCC"), LogicValue::One, DriveStrength::Supply);
        placeParts();
        connectNets();
        addImplicitPower();
        tieResistors();
        if (!errors_.empty()) {
            SortByLine(errors_);
            return std::unexpected(std::move(errors_));
        }
        for (const auto& [pin, net] : pinNet_) {
            const PlacedPart& part = placed_.find(pin.first)->second;
            if (part.component != nullptr) {
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
                    pinNet_.try_emplace({ref.reference, *index}, PinNet{net, decl.name});
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
                    pinNet_.try_emplace({reference, i}, PinNet{netNamed(rail), rail});
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
            const PinNet& rail = IsRail(a->second.name) ? a->second : b->second;
            const PinNet& signal = IsRail(a->second.name) ? b->second : a->second;
            loaded_->circuit.tie(signal.net,
                                 rail.name == "VCC" ? LogicValue::One : LogicValue::Zero,
                                 DriveStrength::Weak);
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
    std::unique_ptr<LoadedCircuit> loaded_;
    std::map<std::string, PlacedPart, std::less<>> placed_;  // by reference
    std::map<std::pair<std::string, PinIndex>, PinNet> pinNet_;
    std::vector<Diagnostic> errors_;
};

}  // namespace

LoadResult BuildCircuit(const Netlist& netlist) { return Builder(netlist).build(); }

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

}  // namespace bw
