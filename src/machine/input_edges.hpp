#pragma once

#include <cstddef>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "machine/knob_setting.hpp"
#include "machine/press.hpp"
#include "machine/typing.hpp"
#include "netlist/builder.hpp"
#include "sim/ids.hpp"
#include "sim/simulator.hpp"
#include "sim/time.hpp"

namespace bw {

// One scripted input at its simulated time: a switch pressed or released, a key typed, a knob
// turned (bw run's --press, --type, --knob; a Vetting Script's).
struct InputEdge {
    Time at;
    ComponentId wake;
    std::function<void()> apply;
};

// The edges of presses, typing and timed knobs on `loaded`, in time order. Knobs with no time are
// set at once. Error: "'SW9' is not a switch", "'J9' is not a keyboard", "cannot type the text on
// 'J3', a keyboard", "'J9' is not a paddle controller".
std::expected<std::vector<InputEdge>, std::string> InputEdges(LoadedCircuit& loaded,
                                                              std::span<const ButtonPress> presses,
                                                              std::span<const KnobSetting> knobs,
                                                              std::span<const Typing> typing);

// Runs `sim` to `end`, a millisecond at a time and stopping at each edge from `next` on to apply
// it (then wake). `slice` after each stop (devices fed, recordings trimmed): false stops the run.
// False: stopped by `slice`. An edge at `end` is applied there, its effect after; later ones are
// left for the next call.
bool RunTo(Simulator& sim, std::span<const InputEdge> edges, std::size_t& next, Time end,
           const std::function<bool()>& slice);

}  // namespace bw
