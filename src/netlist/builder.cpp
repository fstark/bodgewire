#include "netlist/builder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <utility>

#include "netlist/rom_directory.hpp"
#include "netlist/units.hpp"
#include "parts/capacitor.hpp"
#include "parts/catalogue.hpp"
#include "parts/connector.hpp"
#include "parts/matrix_keyboard.hpp"
#include "parts/membrane_keyboard.hpp"
#include "parts/memory_cells.hpp"
#include "parts/resistor_network.hpp"
#include "parts/switch.hpp"
#include "sim/check.hpp"
#include "text/text.hpp"

namespace bw {

namespace {

struct BuiltPart {
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

bool IsResistor(const BuiltPart& part) {
    return part.info != nullptr && part.info->kind == PartKind::Resistor;
}

bool IsCapacitor(const BuiltPart& part) {
    return part.info != nullptr && part.info->kind == PartKind::Capacitor;
}

// A passive Part (resistor, capacitor): pins 1 and 2, only value=, no Component of its own.
bool IsPassive(const BuiltPart& part) { return IsResistor(part) || IsCapacitor(part); }

void SortByLine(std::vector<Diagnostic>& diagnostics) {
    std::stable_sort(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic& a, const Diagnostic& b) { return a.line < b.line; });
}

class Builder {
public:
    explicit Builder(const Netlist& netlist, std::span<const StuckPin> stuck,
                     std::span<const std::string> unpowered, std::span<const StuckCell> cells,
                     std::span<const StuckSwitch> switches)
        : netlist_(netlist), stuck_(stuck), unpowered_(unpowered), cells_(cells),
          switches_(switches), loaded_(std::make_unique<LoadedCircuit>()) {}

    LoadResult build() {
        Circuit& circuit = loaded_->circuit;
        circuit.tie(netNamed("GND"), LogicValue::Zero, DriveStrength::Supply);
        circuit.tie(netNamed("VCC"), LogicValue::One, DriveStrength::Supply);
        placeParts();
        connectNets();
        addImplicitPower();
        wireSwitches();
        wireResistors();
        wireCapacitors();
        if (!errors_.empty()) {
            SortByLine(errors_);
            return std::unexpected(std::move(errors_));
        }
        for (const StuckPin& s : stuck_) {
            const auto it = loaded_->components.find(s.reference);
            Check(it != loaded_->components.end() && s.pin < it->second->pins().size() &&
                      (IsDriver(it->second->pins()[s.pin].kind) ||
                       it->second->pins()[s.pin].kind == PinKind::Input) &&
                      (s.value == LogicValue::Zero || s.value == LogicValue::One),
                  "builder: stuck pin must be an output or input of a placed Component");
        }
        stickCells();
        for (const auto& [pin, net] : pinNet_) {
            const BuiltPart& part = placed_.find(pin.first)->second;
            if (isUnpowered(pin.first)) {
                // no Driver, no Load: the chip's Pins get private Nets at finalize()
            } else if (const StuckPin* s = stuckAt(pin.first, pin.second)) {
                if (!s->input && IsDriver(part.component->pins()[pin.second].kind)) {
                    circuit.tie(net.net, s->value, DriveStrength::Strong);
                }  // a stuck input: on its own Net below
            } else if (notConnected_.contains(pin)) {
                // a switch's second leg: on a Net of its own, as the P4d button's rail end
            } else if (part.component != nullptr) {
                circuit.connect(*part.component, pin.second, net.net);
            }
            loaded_->connections.push_back({pin.first, pin.second, std::string(net.name)});
        }
        // A stuck input reads its value through a Net of its own, listed or not (a dry Joint). A
        // stuck bidirectional or pull-up Pin forces its Net above (unless `input`: the Net is left
        // alone) and reads its value here; Supply, so the chip's own drive onto that Net never
        // fights the tie into X.
        for (const StuckPin& s : stuck_) {
            const Component& component = *loaded_->components.at(s.reference);
            const PinKind kind = component.pins()[s.pin].kind;
            if (!IsReader(kind) || isUnpowered(s.reference)) {
                continue;
            }
            const NetId own =
                circuit.addNet(s.reference + "." + component.pins()[s.pin].name + " stuck");
            circuit.tie(own, s.value,
                        kind == PinKind::Bidirectional || kind == PinKind::PullUp
                            ? DriveStrength::Supply
                            : DriveStrength::Strong);
            circuit.connect(component, s.pin, own);
        }
        for (const PartDecl& decl : netlist_.parts) {
            const BuiltPart& part = placed_.find(decl.reference)->second;
            std::string value = decl.part;
            if (IsPassive(part)) {
                for (const PartParam& param : decl.params) {
                    value = param.value;  // only "value" is accepted
                }
            }
            std::string marking;
            if (const auto freq = std::ranges::find(decl.params, "freq", &PartParam::name);
                freq != decl.params.end()) {
                marking = freq->value;
            }
            loaded_->parts.push_back(
                {decl.reference, part.info, std::move(value), std::move(marking)});
        }
        for (const StuckSwitch& s : switches_) {
            Switch* sw = SwitchOf(*loaded_, s.reference);
            Check(sw != nullptr, "builder: switch Fault on a Part that is not a switch");
            sw->setFault(s.closed ? SwitchFault::StuckClosed : SwitchFault::NeverCloses);
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

    // The Machine checked them (LoadMachine); the Check only guards misuse.
    void stickCells() {
        for (const StuckCell& c : cells_) {
            const auto it = loaded_->components.find(c.reference);
            auto* memory =
                it == loaded_->components.end() ? nullptr : dynamic_cast<MemoryCells*>(it->second);
            const auto bit = memory ? memory->dataBit(c.pin) : std::nullopt;
            Check(bit && c.memory < memory->memories().size() &&
                      (!c.word || *c.word < memory->wordCount(c.memory)) &&
                      (c.value == LogicValue::Zero || c.value == LogicValue::One),
                  "builder: stuck cell must be a data bit of a memory word");
            const bool one = c.value == LogicValue::One;
            if (c.word) {
                memory->stick(c.memory, *c.word, *bit, one);
                continue;
            }
            for (std::size_t word = 0; word < memory->wordCount(c.memory); ++word) {
                memory->stick(c.memory, word, *bit, one);
            }
        }
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
            BuiltPart& part = placed_[decl.reference];
            part = {&decl, FindPart(decl.part), nullptr};
            if (part.info == nullptr) {
                error(decl.line, "unknown part '" + decl.part + "'");
                continue;
            }
            if (IsPassive(part)) {
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

    std::optional<PinIndex> resolvePin(const BuiltPart& part, const PinRef& ref,
                                       std::uint32_t line) {
        if (IsPassive(part)) {
            if (const auto index = PassivePin(ref.pin)) {
                return index;
            }
            error(line, ref.reference + " (" + part.decl->part + ") has no pin " + ref.pin);
            return std::nullopt;
        }
        if (const auto index = FindPin(*part.component, ref.pin)) {
            return index;
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
                const BuiltPart& part = it->second;
                const bool resistor = IsPassive(part);
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
                    if (spec.kind == PinKind::Power && decl.name != PowerPinRail(spec.name)) {
                        error(decl.line, where + " can only be on net " +
                                             std::string(PowerPinRail(spec.name)));
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
                    const std::string_view rail =
                        loaded_->nets.find(PowerPinRail(pins[i].name))->first;
                    pinNet_.try_emplace({reference, i}, NetOfPin{netNamed(rail), rail});
                }
            }
        }
    }

    // Resistors (P5u): a pull (one end on a rail) is tied to its signal Net, unless that Net is
    // joined; a series resistor (both ends on signals) joins its two Nets through the
    // ResistorNetwork, which also takes the pulls of joined Nets (they decide weak fights there).
    void wireResistors() {
        struct Pull {
            NetId signal;
            LogicValue value;
            std::uint32_t ohms;
        };
        struct Series {
            NetId a, b;
            std::uint32_t ohms;
        };
        std::vector<Pull> pulls;
        std::vector<Series> series;
        std::set<NetId> joined;
        for (const PartDecl& decl : netlist_.parts) {
            const BuiltPart& part = placed_.find(decl.reference)->second;
            if (!IsResistor(part)) {
                continue;
            }
            const auto a = pinNet_.find({decl.reference, 0});
            const auto b = pinNet_.find({decl.reference, 1});
            if (a == pinNet_.end() || b == pinNet_.end()) {
                error(decl.line, decl.reference + ": a resistor needs both pins connected");
                continue;
            }
            const auto value = std::ranges::find(decl.params, "value", &PartParam::name);
            if (value == decl.params.end()) {
                error(decl.line, decl.reference + ": a resistor needs value= (ohms)");
                continue;
            }
            const auto ohms = ParseOhms(value->value);
            if (!ohms) {
                error(decl.line, decl.reference + ": " + ohms.error());
                continue;
            }
            const bool railA = IsRail(a->second.name), railB = IsRail(b->second.name);
            if (a->second.net == b->second.net) {
                error(decl.line,
                      decl.reference + ": both ends on net " + std::string(a->second.name));
            } else if (railA && railB) {
                error(decl.line, decl.reference + ": a resistor between two rails");
            } else if (railA || railB) {
                const NetOfPin& rail = railA ? a->second : b->second;
                const NetOfPin& signal = railA ? b->second : a->second;
                pulls.push_back(
                    {signal.net, rail.name == "VCC" ? LogicValue::One : LogicValue::Zero, *ohms});
            } else {
                series.push_back({a->second.net, b->second.net, *ohms});
                joined.insert(a->second.net);
                joined.insert(b->second.net);
            }
        }
        Circuit& circuit = loaded_->circuit;
        for (const Pull& pull : pulls) {
            const auto [it, inserted] = pullOhms_.try_emplace(pull.signal, pull.ohms);
            if (!inserted) {
                it->second = std::min(it->second, pull.ohms);
            }
        }
        for (const Pull& pull : pulls) {
            if (!joined.contains(pull.signal)) {
                circuit.tie(pull.signal, pull.value, DriveStrength::Weak);
            }
        }
        if (joined.empty()) {
            return;
        }
        const std::vector<NetId> nets(joined.begin(), joined.end());  // NetId order
        const auto pinOf = [&](NetId net) {
            return static_cast<PinIndex>(std::ranges::lower_bound(nets, net) - nets.begin());
        };
        std::vector<std::string> names;
        for (const NetId net : nets) {
            names.emplace_back(circuit.netName(net));
        }
        auto& network = circuit.add<ResistorNetwork>(names);
        for (PinIndex p = 0; p < nets.size(); ++p) {
            circuit.connect(network, p, nets[p]);
        }
        for (const Series& s : series) {
            network.join(pinOf(s.a), pinOf(s.b), s.ohms);
        }
        for (const Pull& pull : pulls) {
            if (joined.contains(pull.signal)) {
                network.pull(pinOf(pull.signal), pull.value, pull.ohms);
            }
        }
    }

    // Capacitors (P5v): one between a signal Net with no pull (its input) and a signal Net with a
    // pull resistor (its output) couples each edge into the output for one time constant, the
    // pull's ohms times its own farads. Anywhere else it adds nothing (decoupling across the
    // rails, no pull, pulls on both sides): no error, as a repair can put it anywhere.
    void wireCapacitors() {
        for (const PartDecl& decl : netlist_.parts) {
            const BuiltPart& part = placed_.find(decl.reference)->second;
            if (!IsCapacitor(part)) {
                continue;
            }
            const auto a = pinNet_.find({decl.reference, 0});
            const auto b = pinNet_.find({decl.reference, 1});
            if (a == pinNet_.end() || b == pinNet_.end()) {
                error(decl.line, decl.reference + ": a capacitor needs both pins connected");
                continue;
            }
            const auto value = std::ranges::find(decl.params, "value", &PartParam::name);
            if (value == decl.params.end()) {
                error(decl.line, decl.reference + ": a capacitor needs value= (farads)");
                continue;
            }
            const auto picofarads = ParseFarads(value->value);
            if (!picofarads) {
                error(decl.line, decl.reference + ": " + picofarads.error());
                continue;
            }
            if (a->second.net == b->second.net) {
                error(decl.line,
                      decl.reference + ": both ends on net " + std::string(a->second.name));
                continue;
            }
            const auto pullOf = [&](const NetOfPin& net) -> std::optional<std::uint32_t> {
                if (IsRail(net.name)) {
                    return std::nullopt;
                }
                const auto it = pullOhms_.find(net.net);
                return it == pullOhms_.end() ? std::nullopt : std::optional(it->second);
            };
            const auto pullA = pullOf(a->second), pullB = pullOf(b->second);
            if (IsRail(a->second.name) || IsRail(b->second.name) ||
                pullA.has_value() == pullB.has_value()) {
                continue;
            }
            const PinIndex input = pullA ? 1 : 0;
            const NetId inputNet = pullA ? b->second.net : a->second.net;
            const NetId outputNet = pullA ? a->second.net : b->second.net;
            const Time width = static_cast<Time>(*(pullA ? pullA : pullB) * *picofarads);
            Circuit& circuit = loaded_->circuit;
            auto& capacitor = circuit.add<Capacitor>(decl.reference, input, width);
            circuit.connect(capacitor, input, inputNet);
            circuit.connect(capacitor, 1 - input, outputNet);
        }
    }

    // Switches (P5i): one leg connected to its Net as a never-driving Output and the other left
    // on a Net of its own, as the P4d button's signal and rail ends were (the same Nets and
    // random draws); the SwitchNetwork gets a Switched Pin on every Net a leg is listed on.
    void wireSwitches() {
        struct Legs {
            SwitchContact* member;
            std::array<std::optional<NetId>, 2> nets;
        };
        std::vector<Legs> switches;
        std::map<std::string, std::string, std::less<>> pcTaken;  // PC key → reference
        for (const PartDecl& decl : netlist_.parts) {
            const BuiltPart& part = placed_.find(decl.reference)->second;
            if (part.info == nullptr || part.info->kind != PartKind::Switch ||
                part.component == nullptr) {
                continue;
            }
            auto& sw = static_cast<Switch&>(*part.component);
            if (!sw.pcKey().empty()) {
                const auto [it, added] = pcTaken.emplace(sw.pcKey(), decl.reference);
                if (!added) {
                    error(decl.line, decl.reference + ": PC key " + std::string(sw.pcKey()) +
                                         " is already " + it->second + "'s");
                }
            }
            Legs legs{&sw.contact(), {}};
            for (PinIndex leg = 0; leg < 2; ++leg) {
                if (const auto it = pinNet_.find({decl.reference, leg}); it != pinNet_.end()) {
                    legs.nets[leg] = it->second.net;
                }
            }
            const auto onRail = [&](PinIndex leg) {
                const auto it = pinNet_.find({decl.reference, leg});
                return it != pinNet_.end() && IsRail(it->second.name);
            };
            notConnected_.insert({decl.reference, !onRail(0) || onRail(1) ? 1u : 0u});
            switches.push_back(legs);
        }
        // A key matrix of bare contacts (the BW-2001's, P5q): each key a contact between its
        // row and column Pins' Nets.
        for (const PartDecl& decl : netlist_.parts) {
            const BuiltPart& part = placed_.find(decl.reference)->second;
            auto* keyboard = dynamic_cast<MatrixKeyboard*>(part.component);
            if (keyboard == nullptr) {
                continue;
            }
            const auto netOf = [&](PinIndex pin) -> std::optional<NetId> {
                const auto it = pinNet_.find({decl.reference, pin});
                return it == pinNet_.end() ? std::nullopt : std::optional(it->second.net);
            };
            for (std::size_t k = 0; k < keyboard->keyCount(); ++k) {
                const auto [row, column] = keyboard->contactPins(k);
                switches.push_back({&keyboard->contact(k), {netOf(row), netOf(column)}});
            }
        }
        std::vector<NetId> nets;
        for (const Legs& legs : switches) {
            if (legs.nets[0] && legs.nets[1] && *legs.nets[0] != *legs.nets[1]) {
                nets.push_back(*legs.nets[0]);
                nets.push_back(*legs.nets[1]);
            }
        }
        std::ranges::sort(nets);
        nets.erase(std::unique(nets.begin(), nets.end()), nets.end());
        if (nets.empty()) {
            return;
        }
        Circuit& circuit = loaded_->circuit;
        std::vector<std::string> names;
        for (const NetId net : nets) {
            names.emplace_back(circuit.netName(net));
        }
        auto& network = circuit.add<SwitchNetwork>(names);
        for (PinIndex p = 0; p < nets.size(); ++p) {
            circuit.connect(network, p, nets[p]);
        }
        const auto pinOf = [&](NetId net) {
            return static_cast<PinIndex>(std::ranges::lower_bound(nets, net) - nets.begin());
        };
        for (const Legs& legs : switches) {
            if (legs.nets[0] && legs.nets[1] && *legs.nets[0] != *legs.nets[1]) {
                network.add(*legs.member, pinOf(*legs.nets[0]), pinOf(*legs.nets[1]));
            }
        }
    }

    void warnUnconnectedInputs() {
        for (const auto& [reference, part] : placed_) {
            if (part.component == nullptr) {
                continue;
            }
            const auto pins = part.component->pins();
            for (PinIndex i = 0; i < pins.size(); ++i) {
                if (IsReader(pins[i].kind) && pins[i].family == Family::Ttl &&
                    !pinNet_.contains({reference, i})) {
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
    std::span<const StuckCell> cells_;
    std::span<const StuckSwitch> switches_;
    std::unique_ptr<LoadedCircuit> loaded_;
    std::map<std::string, BuiltPart, std::less<>> placed_;  // by reference
    std::map<std::pair<std::string, PinIndex>, NetOfPin> pinNet_;
    std::set<std::pair<std::string, PinIndex>> notConnected_;
    std::map<NetId, std::uint32_t> pullOhms_;  // signal Net → its smallest pull (wireResistors)
    std::vector<Diagnostic> errors_;
};

}  // namespace

bool IsRail(std::string_view net) { return net == "GND" || net == "VCC"; }

LoadResult BuildCircuit(const Netlist& netlist, const BuildFaults& faults) {
    return Builder(netlist, faults.stuck, faults.unpowered, faults.cells, faults.switches).build();
}

LoadResult LoadNetlist(std::string_view text, std::string file) {
    const auto netlist = ParseNetlist(text, std::move(file));
    if (!netlist) {
        return std::unexpected(netlist.error());
    }
    return BuildCircuit(*netlist);
}

std::expected<Netlist, std::vector<Diagnostic>>
ReadNetlistFile(const std::filesystem::path& path,
                const std::optional<std::filesystem::path>& roms) {
    std::string file = path.filename().string();
    const auto text = ReadTextFile(path);
    if (!text) {
        return std::unexpected(
            std::vector<Diagnostic>{{std::move(file), 0, Severity::Error, "cannot read file"}});
    }
    auto netlist = ParseNetlist(*text, file);
    if (!netlist) {
        return netlist;
    }
    ResolveContentsPaths(*netlist, path.parent_path());
    if (roms) {
        if (const auto applied = ApplyRomDirectory(*netlist, *roms); !applied) {
            return std::unexpected(
                std::vector<Diagnostic>{{std::move(file), 0, Severity::Error, applied.error()}});
        }
    }
    return netlist;
}

LoadResult LoadNetlistFile(const std::filesystem::path& path,
                           const std::optional<std::filesystem::path>& roms) {
    const auto netlist = ReadNetlistFile(path, roms);
    if (!netlist) {
        return std::unexpected(netlist.error());
    }
    return BuildCircuit(*netlist);
}

void ResolveContentsPaths(Netlist& netlist, const std::filesystem::path& directory) {
    for (PartDecl& part : netlist.parts) {
        for (PartParam& param : part.params) {
            if (param.name == "contents" && std::filesystem::path(param.value).is_relative()) {
                param.value = (directory / param.value).lexically_normal().string();
            }
        }
    }
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

std::optional<PinIndex> PassivePin(std::string_view pin) {
    if (pin == "1" || pin == "2") {
        return static_cast<PinIndex>(pin[0] - '1');
    }
    return std::nullopt;
}

std::optional<NetId> PinNet(const LoadedCircuit& loaded, std::string_view reference,
                            std::string_view pin) {
    const auto component = loaded.components.find(reference);
    const auto index =
        component != loaded.components.end() ? FindPin(*component->second, pin) : PassivePin(pin);
    if (!index) {
        return std::nullopt;
    }
    for (const Connection& c : loaded.connections) {
        if (c.reference == reference && c.pin == *index) {
            return loaded.nets.at(c.net);
        }
    }
    if (component == loaded.components.end()) {
        return std::nullopt;  // a passive Part's pins are always listed
    }
    return loaded.circuit.netOf(PinId{ToIndex(component->second->firstPin()) + *index});
}

Switch* SwitchOf(const LoadedCircuit& loaded, std::string_view reference) {
    const auto part = std::ranges::find(loaded.parts, reference, &LoadedPart::reference);
    if (part == loaded.parts.end() || part->info->kind != PartKind::Switch) {
        return nullptr;
    }
    const auto component = loaded.components.find(reference);
    return component == loaded.components.end() ? nullptr : static_cast<Switch*>(component->second);
}

Paddle* PaddleOf(const LoadedCircuit& loaded, std::string_view reference) {
    const auto part = std::ranges::find(loaded.parts, reference, &LoadedPart::reference);
    if (part == loaded.parts.end() || part->info->name != "PADDLE") {
        return nullptr;
    }
    const auto component = loaded.components.find(reference);
    return component == loaded.components.end() ? nullptr : static_cast<Paddle*>(component->second);
}

MatrixKeyboard* MatrixKeyboardOf(const LoadedCircuit& loaded, std::string_view reference) {
    const auto part = std::ranges::find(loaded.parts, reference, &LoadedPart::reference);
    if (part == loaded.parts.end() || part->info->name != "BW2001KB") {
        return nullptr;
    }
    const auto component = loaded.components.find(reference);
    return component == loaded.components.end() ? nullptr
                                                : static_cast<MatrixKeyboard*>(component->second);
}

KeyMatrix* KeyMatrixOf(const LoadedCircuit& loaded, std::string_view reference) {
    if (MatrixKeyboard* matrix = MatrixKeyboardOf(loaded, reference)) {
        return matrix;
    }
    const auto part = std::ranges::find(loaded.parts, reference, &LoadedPart::reference);
    if (part == loaded.parts.end() || part->info->name != "BW80KB") {
        return nullptr;
    }
    const auto component = loaded.components.find(reference);
    return component == loaded.components.end() ? nullptr
                                                : static_cast<MembraneKeyboard*>(component->second);
}

Keyboard* KeyboardOf(const LoadedCircuit& loaded, std::string_view reference) {
    const auto part = std::ranges::find(loaded.parts, reference, &LoadedPart::reference);
    if (part == loaded.parts.end() || part->info->name != "KEYBOARD") {
        return nullptr;
    }
    const auto component = loaded.components.find(reference);
    return component == loaded.components.end() ? nullptr
                                                : static_cast<Keyboard*>(component->second);
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
