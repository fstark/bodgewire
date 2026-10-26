#include "sim/simulator.hpp"

#include <cstdint>
#include <initializer_list>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "sim/circuit.hpp"
#include "sim/primitives.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

struct Drv {
    LogicValue initial;
    std::vector<Stimulus::Step> steps = {};
    Family family = Family::Ttl;
};

// Adds one Stimulus per entry, all driving `net`.
void AddDrivers(Circuit& circuit, NetId net, std::initializer_list<Drv> drivers) {
    int i = 0;
    for (const Drv& d : drivers) {
        auto& s = circuit.add<Stimulus>(std::string(circuit.netName(net)) + "_drv" +
                                            std::to_string(i++),
                                        d.initial, d.steps, d.family);
        circuit.connect(s, Stimulus::kOut, net);
    }
}

// Copies readBit() of its MOS input to its output.
class BitReader final : public Component {
public:
    explicit BitReader(std::string name)
        : Component(std::move(name), {{"IN", PinKind::Input, Family::Mos},
                                      {"OUT", PinKind::Output, Family::Mos}}) {}
    void evaluate(EvalContext& ctx) override { ctx.drive(1, ctx.readBit(0) ? One : Zero, 1); }
};

std::vector<Sample> Samples(const Simulator& sim, NetId net) {
    const auto samples = sim.recorder().samples(net);
    return {samples.begin(), samples.end()};
}

}  // namespace

TEST_CASE("simulator: stimulus steps at exact times", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{Zero, {{10_ns, One}, {25_ns, Zero}}}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.record(net);
    sim.runFor(100_ns);

    const auto samples = Samples(sim, net);
    REQUIRE(samples.size() == 3);
    CHECK((samples[0] == Sample{0, Zero}));
    CHECK((samples[1] == Sample{10_ns, One}));
    CHECK((samples[2] == Sample{25_ns, Zero}));
}

TEST_CASE("simulator: runUntil stops at end and keeps pending events", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{Zero, {{20_ns, One}}}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(15_ns);
    CHECK(sim.now() == 15_ns);
    CHECK(sim.value(net) == Zero);
    sim.runUntil(30_ns);
    CHECK(sim.now() == 30_ns);
    CHECK(sim.value(net) == One);
}

TEST_CASE("simulator: TTL contention reads 0", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{One}, {Zero}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    CHECK(sim.value(net) == Zero);
    CHECK_FALSE(sim.isFloating(net));
}

TEST_CASE("simulator: mixed-family contention reads X", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{One, {}, Family::Ttl}, {Zero, {}, Family::Mos}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    CHECK(sim.value(net) == X);
}

TEST_CASE("simulator: released MOS bus keeps last driven value, floating", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{One, {{10_ns, Z}}, Family::Mos}});
    circuit.finalize();
    Simulator sim(circuit);
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        sim.powerOn(seed);
        CHECK_FALSE(sim.isFloating(net));
        sim.runFor(20_ns);
        CHECK(sim.value(net) == One);
        CHECK(sim.isFloating(net));
    }
}

TEST_CASE("simulator: weak pull-up wins over Z", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    circuit.tie(net, One, DriveStrength::Weak);
    AddDrivers(circuit, net, {{Z}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    CHECK(sim.value(net) == One);
    CHECK_FALSE(sim.isFloating(net));
}

TEST_CASE("simulator: readBit turns X into seeded random bits", "[sim]") {
    Circuit circuit;
    const NetId in = circuit.addNet("in");
    const NetId out = circuit.addNet("out");
    AddDrivers(circuit, in, {{X, {}, Family::Mos}});
    auto& reader = circuit.add<BitReader>("U1");
    circuit.connect(reader, "IN", in);
    circuit.connect(reader, "OUT", out);
    circuit.finalize();
    Simulator sim(circuit);

    std::set<LogicValue> seen;
    for (std::uint64_t seed = 1; seed <= 16; ++seed) {
        sim.powerOn(seed);
        sim.runFor(10_ns);
        const LogicValue first = sim.value(out);
        CHECK(first != X);
        seen.insert(first);
        sim.powerOn(seed);
        sim.runFor(10_ns);
        CHECK(sim.value(out) == first);
    }
    CHECK(seen.size() == 2);
}

namespace {

// 16 random-power-on outputs (BitReaders on an X input) plus a stepping Stimulus.
struct RandomCircuit {
    Circuit circuit;
    std::vector<NetId> outs;
    NetId stim{};

    RandomCircuit() {
        const NetId in = circuit.addNet("in");
        AddDrivers(circuit, in, {{X, {{5_ns, Zero}, {15_ns, X}}, Family::Mos}});
        for (int i = 0; i < 16; ++i) {
            outs.push_back(circuit.addNet("out" + std::to_string(i)));
            auto& reader = circuit.add<BitReader>("U" + std::to_string(i));
            circuit.connect(reader, "IN", in);
            circuit.connect(reader, "OUT", outs.back());
        }
        stim = in;
        circuit.finalize();
    }
};

std::vector<std::vector<Sample>> Run(Simulator& sim, const RandomCircuit& rc,
                                     std::uint64_t seed) {
    sim.powerOn(seed);
    for (const NetId net : rc.outs) {
        sim.record(net);
    }
    sim.runFor(50_ns);
    std::vector<std::vector<Sample>> result;
    for (const NetId net : rc.outs) {
        result.push_back(Samples(sim, net));
    }
    return result;
}

}  // namespace

TEST_CASE("simulator: same seed same recording", "[sim]") {
    RandomCircuit rc;
    Simulator a(rc.circuit);
    Simulator b(rc.circuit);
    CHECK(Run(a, rc, 7) == Run(b, rc, 7));
}

TEST_CASE("simulator: different seed different initial state", "[sim]") {
    RandomCircuit rc;
    Simulator sim(rc.circuit);
    const auto initial = [&](std::uint64_t seed) {
        sim.powerOn(seed);
        std::vector<LogicValue> values;
        for (const NetId net : rc.outs) {
            values.push_back(sim.value(net));
        }
        return values;
    };
    CHECK(initial(1) != initial(2));
    CHECK(initial(3) == initial(3));
}

TEST_CASE("simulator: power-on has no X", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("a");
    const NetId mosFloat = circuit.addNet("mosFloat");
    const NetId out = circuit.addNet("out");
    AddDrivers(circuit, a, {{One, {{3_ns, Zero}}, Family::Mos}});
    auto& reader = circuit.add<BitReader>("U1");
    circuit.connect(reader, "IN", mosFloat);
    circuit.connect(reader, "OUT", out);
    circuit.finalize();
    Simulator sim(circuit);
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        sim.powerOn(seed);
        for (std::uint32_t n = 0; n < circuit.netCount(); ++n) {
            CHECK(sim.value(NetId{n}) != X);
        }
    }
}

TEST_CASE("simulator: powerOn resets time, queue and recordings", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{Zero, {{10_ns, One}}}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.record(net);
    sim.runUntil(10_ns - 1);  // wake applied, change pending at 10 ns

    sim.powerOn(1);
    CHECK(sim.now() == 0);
    CHECK(sim.recorder().nets().empty());
    CHECK(sim.stats().appliedEvents == 0);
    CHECK(sim.value(net) == Zero);
    sim.runUntil(10_ns);
    CHECK(sim.stats().appliedEvents == 1);
    CHECK(sim.value(net) == One);
}

TEST_CASE("simulator: stats count applied events and wakes", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{Zero, {{10_ns, One}, {20_ns, Zero}, {30_ns, Zero}}}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runFor(100_ns);
    CHECK(sim.stats().wakes == 3);
    CHECK(sim.stats().appliedEvents == 2);  // the 30 ns step changes nothing
    CHECK(sim.stats().staleEvents == 0);
}
