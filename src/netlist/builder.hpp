#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "netlist/diagnostic.hpp"
#include "netlist/parser.hpp"
#include "sim/circuit.hpp"

namespace bw {

struct LoadedCircuit {
    Circuit circuit;                                            // finalized
    std::map<std::string, NetId, std::less<>> nets;             // includes GND, VCC
    std::map<std::string, Component*, std::less<>> components;  // chips only
    std::vector<Diagnostic> warnings;
};

// unique_ptr: a Simulator keeps a Circuit&, so the circuit must not move after loading.
using LoadResult = std::expected<std::unique_ptr<LoadedCircuit>, std::vector<Diagnostic>>;

LoadResult BuildCircuit(const Netlist& netlist);  // all errors, sorted by line
LoadResult LoadNetlist(std::string_view text, std::string file);
LoadResult LoadNetlistFile(const std::filesystem::path& path);  // diagnostics name the file only

}  // namespace bw
