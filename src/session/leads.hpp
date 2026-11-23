#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "board/board.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "runner/runner.hpp"

namespace bw {

class Runner;

// What an instrument's setup reads of its Session: the Machine and the current build, and whether
// it is powered. Owned by the Session on the heap (it outlives the Session's moves), updated at
// each build and power change.
struct SessionBuild {
    const Machine* machine = nullptr;  // nullptr: a bare netlist, no instruments
    const LoadedCircuit* circuit = nullptr;
    Runner* runner = nullptr;
    bool powered = false;
};

struct PadLead {                 // a Clip or a Hook
    std::string reference, pin;  // pin by number: "6"
    std::string label;           // "U6.6 1Y"
    Point at;                    // pad, nm
};

// A Machine pad by pin number or name; nullopt for an unknown pad.
std::optional<PadLead> PadLeadOf(const Machine& machine, std::string_view reference,
                                 std::string_view pin);

}  // namespace bw
