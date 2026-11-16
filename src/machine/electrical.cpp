#include "machine/electrical.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "board/copper.hpp"
#include "parts/catalogue.hpp"
#include "parts/connector.hpp"
#include "sim/check.hpp"

namespace bw {

namespace {

class Namer {
public:
    Namer(const Board& board, const Islands& islands) {
        for (std::size_t c = 0; c < board.components.size(); ++c) {
            const BoardComponent& component = board.components[c];
            for (std::size_t p = 0; p < component.pads.size(); ++p) {
                padIsland_[{component.reference, component.pads[p].number}] =
                    islands.padIsland[c][p];
            }
        }
    }

    std::uint32_t island(const std::string& reference, PinIndex pin) const {
        const auto it = padIsland_.find({reference, std::to_string(pin + 1)});
        Check(it != padIsland_.end(), "electrical: Pin without a pad");
        return it->second;
    }

    void preset(std::uint32_t island, std::string name) {
        taken_.insert(name);
        names_[island] = std::move(name);
    }

    std::optional<std::string> nameOf(std::uint32_t island) const {
        const auto it = names_.find(island);
        return it == names_.end() ? std::nullopt : std::optional(it->second);
    }

    // The island's name, or the first free of `net`, net~2, net~3... for a new island.
    const std::string& name(std::uint32_t island, const std::string& net) {
        if (const auto it = names_.find(island); it != names_.end()) {
            return it->second;
        }
        std::string name = net;
        for (int n = 2; taken_.contains(name); ++n) {
            name = net + "~" + std::to_string(n);
        }
        taken_.insert(name);
        return names_[island] = std::move(name);
    }

private:
    std::map<std::pair<std::string, std::string>, std::uint32_t> padIsland_;
    std::map<std::uint32_t, std::string> names_;
    std::set<std::string> taken_;
};

// "NAME~N" → ("NAME", N); a plain name → (name, 1).
std::pair<std::string, int> SplitName(const std::string& name) {
    const std::size_t tilde = name.rfind('~');
    if (tilde == std::string::npos) {
        return {name, 1};
    }
    return {name.substr(0, tilde), std::stoi(name.substr(tilde + 1))};
}

std::optional<PartKind> KindOf(const LoadedCircuit& designCircuit, const std::string& reference) {
    for (const LoadedPart& part : designCircuit.parts) {
        if (part.reference == reference) {
            return part.info->kind;
        }
    }
    return std::nullopt;
}

// A resistor or a capacitor (P5v): two pins, named by the islands they are on, never naming one.
bool IsPassive(const LoadedCircuit& designCircuit, const std::string& reference) {
    const auto kind = KindOf(designCircuit, reference);
    return kind == PartKind::Resistor || kind == PartKind::Capacitor;
}

// Names islands: POWER pads first, then non-resistor, non-power Pins in connection order.
Namer NameIslands(const LoadedCircuit& designCircuit, const Board& board, const Islands& islands) {
    Namer namer(board, islands);
    for (const LoadedPart& part : designCircuit.parts) {
        if (part.info->name == "POWER") {
            namer.preset(namer.island(part.reference, 0), "VCC");
            namer.preset(namer.island(part.reference, 1), "GND");
        }
    }
    for (const Connection& c : designCircuit.connections) {
        if (IsPassive(designCircuit, c.reference) ||
            designCircuit.components.at(c.reference)->pins()[c.pin].kind == PinKind::Power) {
            continue;
        }
        namer.name(namer.island(c.reference, c.pin), c.net);
    }
    return namer;
}

}  // namespace

std::vector<std::string> IslandNetNames(const LoadedCircuit& designCircuit, const Board& board,
                                        const Islands& islands) {
    const Namer namer = NameIslands(designCircuit, board, islands);
    std::vector<std::string> names(islands.islandSize.size());
    for (std::uint32_t i = 0; i < names.size(); ++i) {
        names[i] = namer.nameOf(i).value_or("");
    }
    return names;
}

Netlist ElectricalNetlist(const Netlist& design, const LoadedCircuit& designCircuit,
                          const Board& board, std::span<const PinRef> dry) {
    const Islands islands = FindIslands(board);
    const Namer namer = NameIslands(designCircuit, board, islands);

    std::map<std::string, std::vector<PinRef>> pins;
    // Resistors and capacitors: kept while their ends are on two different islands, not both rails.
    // A switch's legs are named by their islands like a chip's pins.
    std::map<std::string, std::vector<const Connection*>> railParts;
    for (const Connection& c : designCircuit.connections) {
        if (IsPassive(designCircuit, c.reference)) {
            railParts[c.reference].push_back(&c);
            continue;
        }
        if (designCircuit.components.at(c.reference)->pins()[c.pin].kind == PinKind::Power) {
            continue;
        }
        PinRef pin{c.reference, std::to_string(c.pin + 1)};
        if (std::ranges::any_of(dry, [&](const PinRef& d) {
                return d.reference == pin.reference && d.pin == pin.pin;
            })) {
            continue;
        }
        const std::string name = *namer.nameOf(namer.island(c.reference, c.pin));
        pins[name].push_back(std::move(pin));
    }

    std::set<std::string, std::less<>> leftOut;
    for (const auto& [reference, ends] : railParts) {
        Check(ends.size() == 2, "electrical: passive part without two pins");
        const auto a = namer.nameOf(namer.island(reference, ends[0]->pin));
        const auto b = namer.nameOf(namer.island(reference, ends[1]->pin));
        // A pull (one end on a rail), a series resistor (both on signals, P5u) or a capacitor
        // (P5v); bridged across, or between two rails, it does nothing and is left out.
        const bool kept = a && b && *a != *b && !(IsRail(*a) && IsRail(*b));
        if (!kept) {
            leftOut.insert(reference);
            continue;
        }
        pins[*a].push_back({reference, std::to_string(ends[0]->pin + 1)});
        pins[*b].push_back({reference, std::to_string(ends[1]->pin + 1)});
    }

    Netlist result;
    result.file = design.file;
    for (const PartDecl& part : design.parts) {
        if (!leftOut.contains(part.reference)) {
            result.parts.push_back({0, part.reference, part.part, part.params});
        }
    }
    // Design declaration order, each split right after its parent: NetIds match the design's
    // (without dry Joints: a Net of one dry pin is left out).
    std::map<std::string, std::vector<std::pair<int, std::string>>> splits;
    for (const auto& [name, refs] : pins) {
        auto [base, n] = SplitName(name);
        splits[base].emplace_back(n, name);
    }
    for (const NetDecl& decl : design.nets) {
        const auto it = splits.find(decl.name);
        if (it == splits.end()) {
            continue;
        }
        std::ranges::sort(it->second);
        for (const auto& [n, name] : it->second) {
            result.nets.push_back({0, name, std::move(pins.at(name))});
        }
        splits.erase(it);
    }
    for (auto& [base, split] : splits) {  // rails with no Pin listed in the design
        std::ranges::sort(split);
        for (const auto& [n, name] : split) {
            result.nets.push_back({0, name, std::move(pins.at(name))});
        }
    }
    return result;
}

BoardPower FindPower(const LoadedCircuit& designCircuit, const Board& board,
                     const Islands& islands) {
    const Namer namer(board, islands);
    std::map<std::uint32_t, std::set<double>> rails;
    for (const LoadedPart& part : designCircuit.parts) {
        if (part.info->kind != PartKind::Connector) {
            continue;
        }
        const std::vector<PinSpec> pins = ConnectorPins(part.info->name);
        for (PinIndex i = 0; i < pins.size(); ++i) {
            if (const auto volts = ConnectorVolts(part.info->name, pins[i].name)) {
                rails[namer.island(part.reference, i)].insert(*volts);
            }
        }
    }
    BoardPower power;
    power.deadRail = std::ranges::any_of(rails, [](const auto& r) { return r.second.size() > 1; });
    for (const LoadedPart& part : designCircuit.parts) {
        const bool device =
            part.info->kind == PartKind::Connector && ConnectorPowersDevice(part.info->name);
        if (part.info->kind != PartKind::Chip && !device) {
            continue;
        }
        const auto pins = designCircuit.components.at(part.reference)->pins();
        for (PinIndex i = 0; i < pins.size(); ++i) {
            if (pins[i].kind != PinKind::Power) {
                continue;
            }
            const auto want = PowerPinVolts(pins[i].name);
            Check(want.has_value(), "electrical: power Pin without a voltage");
            const auto rail = rails.find(namer.island(part.reference, i));
            if (rail == rails.end() || rail->second != std::set<double>{*want}) {
                power.unpowered.push_back(part.reference);
                break;
            }
        }
    }
    return power;
}

}  // namespace bw
