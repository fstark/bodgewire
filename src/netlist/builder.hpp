#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "netlist/diagnostic.hpp"
#include "netlist/parser.hpp"
#include "parts/button.hpp"
#include "parts/paddle.hpp"
#include "sim/circuit.hpp"

namespace bw {

struct LoadedPart {
    std::string reference;
    const PartInfo* info;
    std::string value;  // resistor "value" parameter, else the Part name
};

struct Connection {
    std::string reference;
    PinIndex pin;
    std::string net;
};

struct LoadedCircuit {
    Circuit circuit;                                            // finalized
    std::map<std::string, NetId, std::less<>> nets;             // includes GND, VCC
    std::map<std::string, Component*, std::less<>> components;  // chips and connectors
    std::vector<LoadedPart> parts;                              // netlist order
    std::vector<Connection> connections;  // incl. implicit power; by reference, pin
    std::vector<Diagnostic> warnings;
};

// unique_ptr: a Simulator keeps a Circuit&, so the circuit must not move after loading.
using LoadResult = std::expected<std::unique_ptr<LoadedCircuit>, std::vector<Diagnostic>>;

// Stuck-at Fault: an output holds its Net at `value` (Zero or One, Strong); an input reads
// `value`, its Net unchanged.
struct StuckPin {
    std::string reference;
    PinIndex pin;
    LogicValue value;
};

// `unpowered`: References of chips (and PADDLE controllers) left out of the Circuit's Nets (outputs
// Z, inputs ignored).
LoadResult BuildCircuit(const Netlist& netlist, std::span<const StuckPin> stuck = {},
                        std::span<const std::string> unpowered = {});  // all errors, by line
LoadResult LoadNetlist(std::string_view text, std::string file);
LoadResult LoadNetlistFile(const std::filesystem::path& path);  // diagnostics name the file only

// Pin by package number ("3") or datasheet name ("1Y"); nullopt if none.
std::optional<PinIndex> FindPin(const Component& component, std::string_view pin);

// Net at a Pin's pad: the Net it is listed on (also for a stuck Pin), else its own Net.
// `pin` is a name or a number; nullopt if the reference or pin is unknown.
std::optional<NetId> PinNet(const LoadedCircuit& loaded, std::string_view reference,
                            std::string_view pin);

// Net on `pin` of the first `part` connector, e.g. ("VIDEO", "SYNC_N"); nullopt if none.
std::optional<NetId> ConnectorNet(const LoadedCircuit& loaded, std::string_view part,
                                  std::string_view pin);

// Voltage of `net` if a connector pin with a declared voltage is on it, else nullopt (logic Net).
std::optional<double> RailVolts(const LoadedCircuit& loaded, NetId net);

bool IsRail(std::string_view net);  // "GND" or "VCC"

// The Button Part `reference` of this build; nullptr if none (unknown, not a Button, or left out
// of an electrical build).
Button* ButtonOf(const LoadedCircuit& loaded, std::string_view reference);

// The PADDLE Part `reference` of this build; nullptr if none (unknown or not a PADDLE).
Paddle* PaddleOf(const LoadedCircuit& loaded, std::string_view reference);

}  // namespace bw
