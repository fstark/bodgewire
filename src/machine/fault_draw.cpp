#include "machine/fault_draw.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "parts/catalogue.hpp"
#include "parts/memory_cells.hpp"

namespace bw {

namespace {

// A number under `n` (n > 0). The modulo's bias is far under one part in a billion here.
std::size_t Under(Rng& rng, std::size_t n) { return static_cast<std::size_t>(rng.next() % n); }

LogicValue Bit(Rng& rng) { return rng.nextBit() ? LogicValue::One : LogicValue::Zero; }

std::string Number(PinIndex pin) { return std::to_string(pin + 1); }

struct Memory {
    std::string reference;
    const MemoryCells* cells;
    std::vector<PinIndex> data;  // its data pins
};

// What the Machine has for each kind, in netlist order.
struct Objects {
    std::vector<PinRef> chipPins;         // every pin of every chip, by number
    std::vector<PinRef> bridgeFirst;      // those with a next pin on the same chip
    std::vector<const Connection*> pads;  // every pad on a Net
    std::map<std::string_view, std::vector<const Connection*>> netPads;
    std::vector<Memory> memories;
    std::vector<std::string> switches;

    explicit Objects(const Machine& machine) {
        const LoadedCircuit& design = machine.designCircuit();
        for (const LoadedPart& part : design.parts) {
            if (part.info->kind == PartKind::Switch) {
                switches.push_back(part.reference);
            }
            if (part.info->kind != PartKind::Chip) {
                continue;
            }
            const Component& component = *design.components.at(part.reference);
            const auto pins = static_cast<PinIndex>(component.pins().size());
            Memory memory{part.reference, dynamic_cast<const MemoryCells*>(&component), {}};
            for (PinIndex pin = 0; pin < pins; ++pin) {
                chipPins.push_back({part.reference, Number(pin)});
                if (pin + 1 < pins) {
                    bridgeFirst.push_back({part.reference, Number(pin)});
                }
                if (memory.cells != nullptr && memory.cells->dataBit(pin)) {
                    memory.data.push_back(pin);
                }
            }
            if (!memory.data.empty()) {
                memories.push_back(std::move(memory));
            }
        }
        for (const Connection& connection : design.connections) {
            pads.push_back(&connection);
            netPads[connection.net].push_back(&connection);
        }
    }
};

// One draw of `kind`; nullopt when the draw itself cannot be (a pad alone on its Net).
std::optional<FaultDecl> Draw(const Objects& objects, FaultKind kind, Rng& rng) {
    switch (kind) {
    case FaultKind::Stuck: {
        const PinRef& pin = objects.chipPins[Under(rng, objects.chipPins.size())];
        return StuckAtDecl{0, pin.reference, pin.pin, Bit(rng)};
    }
    case FaultKind::Joint:
        return JointDecl{0, objects.chipPins[Under(rng, objects.chipPins.size())]};
    case FaultKind::Bridge: {
        const PinRef& first = objects.bridgeFirst[Under(rng, objects.bridgeFirst.size())];
        return BridgeDecl{0, first, {first.reference, std::to_string(std::stoi(first.pin) + 1)}};
    }
    case FaultKind::Crack: {
        const Connection& a = *objects.pads[Under(rng, objects.pads.size())];
        const auto& net = objects.netPads.at(a.net);
        const Connection& b = *net[Under(rng, net.size())];
        const double percent = static_cast<double>(Under(rng, 10001)) / 100.0;
        if (&a == &b) {
            return std::nullopt;
        }
        return CrackDecl{0, {a.reference, Number(a.pin)}, {b.reference, Number(b.pin)}, percent};
    }
    case FaultKind::Column:
    case FaultKind::Cell: {
        const Memory& memory = objects.memories[Under(rng, objects.memories.size())];
        const PinIndex pin = memory.data[Under(rng, memory.data.size())];
        const auto names = memory.cells->memories();
        const std::size_t which = Under(rng, names.size());
        CellDecl cell{0, {memory.reference, Number(pin)}, std::nullopt, Bit(rng)};
        if (names.size() > 1) {
            cell.memory = names[which];
        }
        if (kind == FaultKind::Cell) {
            cell.word = static_cast<std::uint32_t>(Under(rng, memory.cells->wordCount(which)));
        }
        return cell;
    }
    case FaultKind::Switch: {
        const std::string& reference = objects.switches[Under(rng, objects.switches.size())];
        return SwitchDecl{0, reference, rng.nextBit()};
    }
    }
    return std::nullopt;
}

bool Has(const Objects& objects, FaultKind kind) {
    switch (kind) {
    case FaultKind::Stuck:
    case FaultKind::Joint:
        return !objects.chipPins.empty();
    case FaultKind::Bridge:
        return !objects.bridgeFirst.empty();
    case FaultKind::Crack:
        return !objects.chipPins.empty() && !objects.pads.empty();
    case FaultKind::Column:
    case FaultKind::Cell:
        return !objects.memories.empty();
    case FaultKind::Switch:
        return !objects.chipPins.empty() && !objects.switches.empty();
    }
    return false;
}

}  // namespace

std::optional<FaultKind> DrawKind(const Machine& machine, Rng& rng) {
    const Objects objects(machine);
    std::array<int, 7> weights = kFaultWeights;
    int total = 0;
    for (std::size_t kind = 0; kind < weights.size(); ++kind) {
        if (!Has(objects, static_cast<FaultKind>(kind))) {
            weights[kind] = 0;
        }
        total += weights[kind];
    }
    if (total == 0) {
        return std::nullopt;
    }
    auto pick = static_cast<int>(Under(rng, static_cast<std::size_t>(total)));
    for (std::size_t kind = 0; kind < weights.size(); ++kind) {
        if (pick < weights[kind]) {
            return static_cast<FaultKind>(kind);
        }
        pick -= weights[kind];
    }
    return std::nullopt;
}

std::optional<FaultDecl> DrawFault(const Machine& machine, FaultKind kind, Rng& rng) {
    const Objects objects(machine);
    if (!Has(objects, kind)) {
        return std::nullopt;
    }
    for (int tries = 0; tries < kDrawTries; ++tries) {
        if (auto fault = Draw(objects, kind, rng); fault && machine.check(*fault)) {
            return fault;
        }
    }
    return std::nullopt;
}

}  // namespace bw
