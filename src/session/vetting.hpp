#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

#include "machine/machine.hpp"
#include "machine/puzzle.hpp"
#include "machine/vetting_script.hpp"
#include "netlist/builder.hpp"

namespace bw {

// Vetting (P8d): whether a Fault shows on a Machine. The Machine is run headless from power-on
// with its Vetting Script's inputs, and what it shows at each capture time is compared with what
// the Golden Machine shows there.

// What a Machine shows at one capture time: its TV's frame, or its lamps.
struct Shown {
    bool locked = false;       // the TV has a picture
    std::vector<float> cells;  // TvFrame::cells; empty on a Machine vetted on its lamps
    std::vector<float> lamps;  // each watched lamp's brightness; empty on a Machine with a TV
};

// The outputs a Machine is vetted on, decided once on its fault-free build.
struct Watched {
    bool tv = false;                                         // FindTvNets found one: the TV alone
    std::vector<std::pair<std::string, std::size_t>> lamps;  // else each lamp: reference, lamp
};
Watched WatchedOf(const LoadedCircuit& golden);

inline constexpr int kShownCells = 100;      // TV cells that differ: fewer do not show
inline constexpr float kCellMargin = 0.25f;  // a cell differs when off by more
inline constexpr float kLampMargin = 0.25f;  // of the brightest golden lamp: a lamp differs past it

// The golden picture lost, kShownCells cells off by more than kCellMargin, or a lamp off by more
// than kLampMargin of `brightest`.
bool Shows(const Shown& golden, const Shown& mine, float brightest);

// `loaded` run from power-on (seed 1) with the script's inputs, what it shows taken at each
// capture time. With `golden`, stops at the first that Shows and returns those so far. A watched
// lamp this build has lost reads dark, a lost TV is no picture. Stopped by `stop`: nullopt.
// Error: InputEdges'.
std::expected<std::optional<std::vector<Shown>>, std::string>
RunScript(LoadedCircuit& loaded, const VettingScript& script, const Watched& watched,
          const std::vector<Shown>* golden, float brightest, std::stop_token stop);

// A Machine and what it shows fault-free: whether a Fault shows on it.
class Vetting {
public:
    // `machine` fault-free; runs it once. nullopt: stopped. Error: the script's (an unknown
    // switch), a Machine with no TV and no lamp, or a Golden Machine with no picture at a
    // capture time.
    static std::expected<std::optional<Vetting>, std::string>
    Make(Machine machine, VettingScript script, std::stop_token stop = {});

    const Machine& machine() const { return machine_; }  // fault-free between two shows()
    const VettingScript& script() const { return script_; }
    const Watched& watched() const { return watched_; }
    const std::vector<Shown>& golden() const { return golden_; }  // one a capture time
    float brightest() const { return brightest_; }  // the golden's brightest lamp; 1 with none lit

    // Whether the Fault shows; false for one the Machine refuses or cannot build. nullopt:
    // stopped. The Machine is fault-free again afterwards.
    std::optional<bool> shows(const FaultDecl& fault, std::stop_token stop = {});

private:
    Vetting(Machine machine, VettingScript script, Watched watched, std::vector<Shown> golden,
            float brightest);

    Machine machine_;
    VettingScript script_;
    Watched watched_;
    std::vector<Shown> golden_;
    float brightest_;
};

}  // namespace bw
