#include "instruments/logic_probe.hpp"

#include <algorithm>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "sim/circuit.hpp"
#include "sim/fixtures.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

// One Net driven by a Stimulus with `steps`, or undriven when `initial` is Z.
struct Bench {
    Circuit circuit;
    NetId net;
    explicit Bench(LogicValue initial, std::vector<Stimulus::Step> steps = {}) {
        net = circuit.addNet("n");
        if (initial != Z) {
            auto& s = circuit.add<Stimulus>("S", initial, std::move(steps), Family::Ttl);
            circuit.connect(s, Stimulus::kOut, net);
        }
        circuit.finalize();
    }
};

// Runs in 1 ms slices to `end`, updating the probe (and trimming if asked).
void RunTo(Simulator& sim, LogicProbe& probe, Time end, bool trim = false) {
    while (sim.now() < end) {
        sim.runUntil(std::min(end, sim.now() + 1_ms));
        probe.update();
        if (trim) {
            sim.trimRecordings();
        }
    }
}

}  // namespace

TEST_CASE("logic probe: steady high", "[instruments]") {
    Bench bench(One);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    LogicProbe probe(sim, bench.net);
    RunTo(sim, probe, 10_ms);
    CHECK(FormatLamps(probe.lamps()) == "HIGH");
}

TEST_CASE("logic probe: pulse stretches 100 ms", "[instruments]") {
    Bench bench(Zero, {{1_ms, One}});
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    LogicProbe probe(sim, bench.net);
    RunTo(sim, probe, 100_ms);
    CHECK(FormatLamps(probe.lamps()) == "HIGH PULSE");
    RunTo(sim, probe, 101'100_us);
    CHECK(FormatLamps(probe.lamps()) == "HIGH");
}

TEST_CASE("logic probe: floating net lights nothing", "[instruments]") {
    Bench bench(Z);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    LogicProbe probe(sim, bench.net);
    RunTo(sim, probe, 1_ms);
    CHECK(FormatLamps(probe.lamps()) == "-");
}

TEST_CASE("logic probe: survives trimRecordings", "[instruments]") {
    const std::vector<Stimulus::Step> steps = {
        {500_us, One}, {2'300_us, Zero}, {2'301_us, One}, {150_ms, Zero}};
    Bench trimmedBench(Zero, steps), keptBench(Zero, steps);
    Simulator trimmed(trimmedBench.circuit), kept(keptBench.circuit);
    trimmed.powerOn(1);
    kept.powerOn(1);
    LogicProbe trimmedProbe(trimmed, trimmedBench.net), keptProbe(kept, keptBench.net);
    for (Time t = 1_ms; t <= 300_ms; t += 1_ms) {
        RunTo(trimmed, trimmedProbe, t, true);
        RunTo(kept, keptProbe, t);
        INFO("t " << t);
        REQUIRE(FormatLamps(trimmedProbe.lamps()) == FormatLamps(keptProbe.lamps()));
    }
    CHECK(FormatLamps(keptProbe.lamps()) == "LOW");
}

TEST_CASE("logic probe: FormatLamps", "[instruments]") {
    CHECK(FormatLamps({}) == "-");
    CHECK(FormatLamps({.high = true}) == "HIGH");
    CHECK(FormatLamps({.low = true, .pulse = true}) == "LOW PULSE");
    CHECK(FormatLamps({.pulse = true}) == "PULSE");
}

TEST_CASE("probe state: first Sample is the level, no PULSE", "[instruments]") {
    ProbeState state;
    CHECK(FormatLamps(state.lamps(0)) == "-");
    state.feed({5_ms, One});
    CHECK(FormatLamps(state.lamps(5_ms)) == "HIGH");
    ProbeState low;
    low.feed({5_ms, Zero});
    CHECK(FormatLamps(low.lamps(5_ms)) == "LOW");
}

TEST_CASE("probe state: change lights PULSE for 100 ms", "[instruments]") {
    ProbeState state;
    state.feed({0, Zero});
    state.feed({10_ms, One});
    CHECK(FormatLamps(state.lamps(10_ms)) == "HIGH PULSE");
    CHECK(FormatLamps(state.lamps(109_ms)) == "HIGH PULSE");
    CHECK(FormatLamps(state.lamps(110_ms)) == "HIGH");
}

TEST_CASE("probe state: floating-only change no PULSE", "[instruments]") {
    ProbeState state;
    state.feed({0, One});
    state.feed({10_ms, One, true});
    CHECK(FormatLamps(state.lamps(10_ms)) == "-");
}

TEST_CASE("probe state: floating is dark", "[instruments]") {
    ProbeState state;
    state.feed({0, One, true});
    CHECK(FormatLamps(state.lamps(1_ms)) == "-");
}

TEST_CASE("probe state: reset", "[instruments]") {
    ProbeState state;
    state.feed({0, Zero});
    state.feed({10_ms, One});
    state.reset();
    CHECK(FormatLamps(state.lamps(10_ms)) == "-");
    state.feed({0, Zero});
    CHECK(FormatLamps(state.lamps(0)) == "LOW");
}
