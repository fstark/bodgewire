#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "board/board.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "runner/runner.hpp"

namespace bw {

// What the instruments' setups and the input devices read of their Session: the Machine and the
// current build, and whether it is powered. Owned by the Session on the heap (it outlives the
// Session's moves), updated at each build and power change.
struct SessionBuild {
    const Machine* machine = nullptr;  // nullptr: a bare netlist, no instruments
    const LoadedCircuit* circuit = nullptr;
    Runner* runner = nullptr;
    bool powered = false;
    // What lists the devices: the Machine's design Circuit, else (a bare netlist) the build's.
    const LoadedCircuit& design() const { return machine ? machine->designCircuit() : *circuit; }
};

struct PadLead {                 // a Clip or a Hook
    std::string reference, pin;  // pin by number: "6"
    std::string label;           // "U6.6 1Y"; a resistor's, a capacitor's: "R6.2"
    Point at;                    // pad, nm
    PinIndex index = 0;          // pin number − 1
    bool chip = true;            // false: a passive Part's (no Component, no pin name)
};

// A Machine pad by pin number or name (a resistor's, a capacitor's: by number); nullopt for an
// unknown pad.
std::optional<PadLead> PadLeadOf(const Machine& machine, std::string_view reference,
                                 std::string_view pin);

}  // namespace bw
