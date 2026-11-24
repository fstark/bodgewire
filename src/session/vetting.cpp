#include "session/vetting.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <format>

#include "devices/tv.hpp"
#include "devices/tv_input.hpp"
#include "machine/input_edges.hpp"
#include "runner/led_input.hpp"
#include "session/session.hpp"
#include "sim/simulator.hpp"

namespace bw {

Watched WatchedOf(const LoadedCircuit& golden) {
    Watched watched;
    watched.tv = FindTvNets(golden).has_value();
    if (!watched.tv) {
        for (const LedNets& led : FindLedNets(golden)) {
            watched.lamps.emplace_back(led.reference, led.lamp);
        }
    }
    return watched;
}

bool Shows(const Shown& golden, const Shown& mine, float brightest) {
    if (golden.locked && !mine.locked) {
        return true;
    }
    if (golden.cells.size() != mine.cells.size() || golden.lamps.size() != mine.lamps.size()) {
        return true;
    }
    int differing = 0;
    for (std::size_t i = 0; i < golden.cells.size(); ++i) {
        if (std::abs(golden.cells[i] - mine.cells[i]) > kCellMargin && ++differing >= kShownCells) {
            return true;
        }
    }
    const float margin = kLampMargin * brightest;
    for (std::size_t i = 0; i < golden.lamps.size(); ++i) {
        if (std::abs(golden.lamps[i] - mine.lamps[i]) > margin) {
            return true;
        }
    }
    return false;
}

std::expected<std::optional<std::vector<Shown>>, std::string>
RunScript(LoadedCircuit& loaded, const VettingScript& script, const Watched& watched,
          const std::vector<Shown>* golden, float brightest, std::stop_token stop) {
    const auto edges = InputEdges(loaded, script.presses, script.knobs, script.typing);
    if (!edges) {
        return std::unexpected(edges.error());
    }
    Simulator sim(loaded.circuit);
    sim.powerOn(1);  // bw_ui's seed: untouched, the run the player sees
    std::optional<TvInput> tv;
    std::optional<LedInput> leds;
    std::vector<std::optional<std::size_t>> lampAt;  // each watched lamp among this build's
    if (watched.tv) {
        if (const auto nets = FindTvNets(loaded)) {
            tv.emplace(sim, *nets);
        }
    } else {
        std::vector<LedNets> nets = FindLedNets(loaded);
        for (const auto& [reference, lamp] : watched.lamps) {
            const auto found = std::ranges::find_if(nets, [&](const LedNets& led) {
                return led.reference == reference && led.lamp == lamp;
            });
            lampAt.push_back(found == nets.end()
                                 ? std::nullopt
                                 : std::optional(static_cast<std::size_t>(found - nets.begin())));
        }
        leds.emplace(sim, std::move(nets));
    }
    const auto slice = [&] {
        if (tv) {
            tv->update();
        }
        if (leds) {
            leds->update();
        }
        sim.trimRecordings();
        return !stop.stop_requested();
    };

    std::vector<Shown> shown;
    std::size_t next = 0;
    for (std::size_t capture = 0; capture < script.captures.size(); ++capture) {
        if (!RunTo(sim, *edges, next, script.captures[capture], slice)) {
            return std::optional<std::vector<Shown>>{};
        }
        Shown now;
        if (watched.tv) {
            now.locked = tv && tv->tv().isLocked();
            now.cells = tv ? tv->tv().completedFrame().cells : TvFrame{}.cells;
        } else {
            for (const std::optional<std::size_t>& lamp : lampAt) {
                now.lamps.push_back(lamp ? leds->brightness(*lamp) : 0.0f);
            }
        }
        shown.push_back(std::move(now));
        if (golden != nullptr && Shows((*golden)[capture], shown.back(), brightest)) {
            break;  // it shows: the later capture times add nothing
        }
    }
    return std::optional(std::move(shown));
}

Vetting::Vetting(Machine machine, VettingScript script, Watched watched, std::vector<Shown> golden,
                 float brightest)
    : machine_(std::move(machine)), script_(std::move(script)), watched_(std::move(watched)),
      golden_(std::move(golden)), brightest_(brightest) {}

std::expected<std::optional<Vetting>, std::string>
Vetting::Make(Machine machine, VettingScript script, std::stop_token stop) {
    auto built = machine.build();
    if (!built) {
        return std::unexpected(built.error().empty() ? std::string("the Machine does not build")
                                                     : built.error().front().message);
    }
    Watched watched = WatchedOf(**built);
    if (!watched.tv && watched.lamps.empty()) {
        return std::unexpected("the Machine has no TV and no lamp to watch");
    }
    auto golden = RunScript(**built, script, watched, nullptr, 1.0f, stop);
    if (!golden) {
        return std::unexpected(golden.error());
    }
    if (!*golden) {
        return std::optional<Vetting>{};
    }
    float brightest = 0;
    for (std::size_t capture = 0; capture < (*golden)->size(); ++capture) {
        const Shown& shown = (**golden)[capture];
        if (watched.tv && !shown.locked) {
            return std::unexpected(
                std::format("the fault-free Machine has no picture at its capture {} ({} ms)",
                            capture + 1, script.captures[capture] / 1'000'000'000));
        }
        for (const float lamp : shown.lamps) {
            brightest = std::max(brightest, lamp);
        }
    }
    return std::optional(Vetting(std::move(machine), std::move(script), std::move(watched),
                                 std::move(**golden), brightest > 0 ? brightest : 1.0f));
}

std::optional<bool> Vetting::shows(const FaultDecl& fault, std::stop_token stop) {
    std::optional<bool> result = false;
    // A Check in the builder or the kernel, on a Fault nobody tried by hand, is a Fault that does
    // not show. One simulation, on the maker's thread or bw's: never around a frame loop (P8b).
    try {
        if (machine_.inject(fault)) {
            if (auto built = machine_.build()) {
                const auto shown =
                    RunScript(**built, script_, watched_, &golden_, brightest_, stop);
                if (shown && !*shown) {
                    result = std::nullopt;
                } else if (shown && !(*shown)->empty()) {
                    result = Shows(golden_[(*shown)->size() - 1], (*shown)->back(), brightest_);
                }
            }
        }
    } catch (const std::exception&) {
        result = false;
    }
    machine_.clearFaults();
    return result;
}

}  // namespace bw
