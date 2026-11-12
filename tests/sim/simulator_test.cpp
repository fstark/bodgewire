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
        auto& s =
            circuit.add<Stimulus>(std::string(circuit.netName(net)) + "_drv" + std::to_string(i++),
                                  d.initial, d.steps, d.family);
        circuit.connect(s, Stimulus::kOut, net);
    }
}

// Copies readBit() of its MOS input to its output.
class BitReader final : public Component {
public:
    explicit BitReader(std::string name)
        : Component(std::move(name),
                    {{"IN", PinKind::Input, Family::Mos}, {"OUT", PinKind::Output, Family::Mos}}) {}
    void evaluate(EvalContext& ctx) override { ctx.drive(1, ctx.readBit(0) ? One : Zero, 1); }
};

std::vector<Sample> Samples(const Simulator& sim, NetId net) {
    const auto samples = sim.recorder().samples(net);
    return {samples.begin(), samples.end()};
}

// Stores what it reads on Pin A at each evaluation.
class Probe final : public Component {
public:
    explicit Probe(std::string name)
        : Component(std::move(name), {{"A", PinKind::Input, Family::Ttl}}) {}
    void evaluate(EvalContext& ctx) override { a = ctx.read(0); }
    LogicValue a = Z;
};

}  // namespace

TEST_CASE("simulator: read sees X, 0, 1 after net changes", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{Zero, {{10_ns, One}, {20_ns, X}, {30_ns, Zero}}}});
    auto& probe = circuit.add<Probe>("U1");
    circuit.connect(probe, "A", net);
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    CHECK(probe.a == Zero);
    sim.runUntil(15_ns);
    CHECK(probe.a == One);
    sim.runUntil(25_ns);
    CHECK(probe.a == X);
    sim.runUntil(35_ns);
    CHECK(probe.a == Zero);
}

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

TEST_CASE("simulator: recording continues after trimRecordings", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{Zero, {{10_ns, One}, {25_ns, Zero}, {40_ns, One}}}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.record(net);
    sim.runFor(30_ns);
    sim.trimRecordings();
    CHECK(Samples(sim, net) == std::vector<Sample>{{25_ns, Zero}});
    sim.runFor(30_ns);
    CHECK(Samples(sim, net) == std::vector<Sample>{{25_ns, Zero}, {40_ns, One}});
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

TEST_CASE("simulator: recording an undriven Net starts floating", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{Z}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.record(net);
    const auto samples = Samples(sim, net);
    REQUIRE(samples.size() == 1);
    CHECK(samples[0].floating);
}

TEST_CASE("simulator: floating-only change is recorded", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("n");
    AddDrivers(circuit, net, {{One, {{10_ns, Z}}, Family::Mos}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.record(net);
    sim.runFor(20_ns);
    CHECK(Samples(sim, net) == std::vector<Sample>{{0, One, false}, {10_ns, One, true}});
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

std::vector<std::vector<Sample>> Run(Simulator& sim, const RandomCircuit& rc, std::uint64_t seed) {
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

namespace {

// Counts its evaluations and remembers the time of the last one.
class Counter final : public Component {
public:
    explicit Counter(std::string name) : Component(std::move(name), {}) {}
    void evaluate(EvalContext& ctx) override {
        ++count;
        at = ctx.now();
    }
    int count = 0;
    Time at = -1;
};

}  // namespace

TEST_CASE("simulator: wake evaluates at now in the next runUntil", "[sim]") {
    Circuit circuit;
    auto& counter = circuit.add<Counter>("C");
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    REQUIRE(counter.count == 1);  // power-on evaluation
    sim.runUntil(5_ns);
    sim.wake(counter.id());
    CHECK(counter.count == 1);
    sim.runUntil(10_ns);
    CHECK(counter.count == 2);
    CHECK(counter.at == 5_ns);
    CHECK(sim.stats().wakes == 1);
    sim.wake(counter.id(), 3_ns);
    sim.runUntil(20_ns);
    CHECK(counter.count == 3);
    CHECK(counter.at == 13_ns);
}

namespace {

// Stimulus → Buffer → out. Settles for 50 ns, then records `out`.
struct BufferRun {
    Circuit circuit;
    NetId in{};
    NetId out{};

    BufferRun(Delays delays, std::vector<Stimulus::Step> steps) {
        in = circuit.addNet("in");
        out = circuit.addNet("out");
        auto& stimulus = circuit.add<Stimulus>("S", Zero, std::move(steps));
        auto& gate = circuit.add<Gate>("U1", GateKind::Buffer, 1, delays);
        circuit.connect(stimulus, Stimulus::kOut, in);
        circuit.connect(gate, "I0", in);
        circuit.connect(gate, gate.output(), out);
        circuit.finalize();
    }

    std::vector<Sample> run(Simulator& sim) {
        sim.powerOn(1);
        sim.runUntil(50_ns);
        sim.record(out);
        sim.runUntil(500_ns);
        return Samples(sim, out);
    }
};

}  // namespace

TEST_CASE("inertial: pulse shorter than delay is swallowed", "[sim]") {
    BufferRun br({10_ns, 10_ns}, {{100_ns, One}, {105_ns, Zero}});
    Simulator sim(br.circuit);
    const auto samples = br.run(sim);
    REQUIRE(samples.size() == 1);
    CHECK(samples[0].value == Zero);
    CHECK(sim.stats().staleEvents == 1);
}

TEST_CASE("inertial: pulse longer than delay passes", "[sim]") {
    BufferRun br({10_ns, 10_ns}, {{100_ns, One}, {120_ns, Zero}});
    Simulator sim(br.circuit);
    const auto samples = br.run(sim);
    REQUIRE(samples.size() == 3);
    CHECK((samples[1] == Sample{110_ns, One}));
    CHECK((samples[2] == Sample{130_ns, Zero}));
}

TEST_CASE("inertial: pending change cancelled when input returns", "[sim]") {
    // Output 0, change to 1 pending, input back to 0: nothing new scheduled, one stale event.
    BufferRun br({10_ns, 10_ns}, {{100_ns, One}, {101_ns, Zero}});
    Simulator sim(br.circuit);
    sim.powerOn(1);
    sim.runUntil(50_ns);
    const SimStats before = sim.stats();
    sim.runUntil(500_ns);
    CHECK(sim.value(br.out) == Zero);
    CHECK(sim.stats().staleEvents - before.staleEvents == 1);
    CHECK(sim.stats().appliedEvents - before.appliedEvents == 2);  // the two Stimulus steps
}

TEST_CASE("inertial: rise and fall delays differ", "[sim]") {
    BufferRun br({3_ns, 7_ns}, {{100_ns, One}, {200_ns, Zero}});
    Simulator sim(br.circuit);
    const auto samples = br.run(sim);
    REQUIRE(samples.size() == 3);
    CHECK((samples[1] == Sample{103_ns, One}));
    CHECK((samples[2] == Sample{207_ns, Zero}));
}

TEST_CASE("simulator: simultaneous changes give no glitch", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("a");
    const NetId b = circuit.addNet("b");
    const NetId y = circuit.addNet("y");
    AddDrivers(circuit, a, {{Zero, {{100_ns, One}, {200_ns, Zero}}}});
    AddDrivers(circuit, b, {{One, {{100_ns, Zero}, {200_ns, One}}}});
    auto& gate = circuit.add<Gate>("U1", GateKind::And, 2, Delays{1_ns, 1_ns});
    circuit.connect(gate, "I0", a);
    circuit.connect(gate, "I1", b);
    circuit.connect(gate, gate.output(), y);
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(50_ns);
    sim.record(y);
    sim.runUntil(300_ns);
    const auto samples = Samples(sim, y);
    REQUIRE(samples.size() == 1);
    CHECK(samples[0].value == Zero);
}

TEST_CASE("simulator: floating TTL input reads 1", "[sim]") {
    Circuit circuit;
    const NetId in = circuit.addNet("in");
    const NetId out = circuit.addNet("out");
    AddDrivers(circuit, in, {{Z}});
    auto& gate = circuit.add<Gate>("U1", GateKind::Buffer, 1, Delays{1_ns, 1_ns});
    circuit.connect(gate, "I0", in);
    circuit.connect(gate, gate.output(), out);
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runFor(10_ns);
    CHECK(sim.value(in) == One);
    CHECK(sim.isFloating(in));
    CHECK(sim.value(out) == One);
    CHECK_FALSE(sim.isFloating(out));
}

namespace {

// Records, at each evaluation, whether Pin A fell at that timestamp.
class FallProbe final : public Component {
public:
    explicit FallProbe(std::string name)
        : Component(std::move(name), {{"A", PinKind::Input, Family::Ttl}}) {}
    void evaluate(EvalContext& ctx) override {
        if (ctx.fell(0)) {
            falls.push_back(ctx.now());
        }
    }
    std::vector<Time> falls;
};

// One MOS bidirectional Pin D and a TTL input EN: while EN is 1 it drives `value` on D, else Z.
// Records what it reads on D (and before) at each evaluation.
class BusChip final : public Component {
public:
    explicit BusChip(std::string name, LogicValue value)
        : Component(std::move(name), {{"D", PinKind::Bidirectional, Family::Mos},
                                      {"EN", PinKind::Input, Family::Ttl}}),
          value_(value) {}
    void evaluate(EvalContext& ctx) override {
        ++evaluations;
        reads.push_back(ctx.read(0));
        ctx.drive(0, ctx.read(1) == One ? value_ : Z, 5_ns);
    }
    std::vector<LogicValue> reads;
    int evaluations = 0;

private:
    LogicValue value_;
};

// Copies readBitBefore() of its MOS input to its output on each evaluation.
class BeforeReader final : public Component {
public:
    explicit BeforeReader(std::string name)
        : Component(std::move(name),
                    {{"IN", PinKind::Input, Family::Mos}, {"OUT", PinKind::Output, Family::Mos}}) {}
    void evaluate(EvalContext& ctx) override {
        bits.push_back(ctx.readBitBefore(0));
        ctx.drive(1, bits.back() ? One : Zero, 1);
    }
    std::vector<bool> bits;
};

// Draws 16 randomBit()s on each rising edge of CLK.
class RandomDrawer final : public Component {
public:
    explicit RandomDrawer(std::string name)
        : Component(std::move(name), {{"CLK", PinKind::Input, Family::Ttl, true}}) {}
    void evaluate(EvalContext& ctx) override {
        if (ctx.rose(0)) {
            for (int i = 0; i < 16; ++i) {
                bits.push_back(ctx.randomBit());
            }
        }
    }
    std::vector<bool> bits;
};

}  // namespace

TEST_CASE("simulator: a bidirectional pin starts released", "[sim]") {
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        Circuit circuit;
        const NetId bus = circuit.addNet("BUS");
        const NetId en = circuit.addNet("EN");
        auto& chip = circuit.add<BusChip>("U1", One);
        circuit.connect(chip, "D", bus);
        circuit.connect(chip, "EN", en);
        AddDrivers(circuit, en, {{Zero}});
        AddDrivers(circuit, bus, {{Zero, {{10_ns, Z}}, Family::Ttl}});
        circuit.finalize();
        Simulator sim(circuit);
        sim.powerOn(seed);
        CHECK(sim.value(bus) == Zero);  // the Stimulus alone: the chip drives nothing yet
        CHECK(chip.reads.front() == Zero);
    }
}

TEST_CASE("simulator: a bidirectional pin drives and reads its net", "[sim]") {
    Circuit circuit;
    const NetId bus = circuit.addNet("BUS");
    const NetId en = circuit.addNet("EN");
    auto& chip = circuit.add<BusChip>("U1", One);
    circuit.connect(chip, "D", bus);
    circuit.connect(chip, "EN", en);
    AddDrivers(circuit, en, {{Zero, {{100_ns, One}, {200_ns, Zero}}}});
    // Another Driver on the bus: 0 until 50 ns, released, then 0 again from 150 ns (a fight).
    AddDrivers(circuit, bus, {{Zero, {{50_ns, Z}, {150_ns, Zero}, {180_ns, Z}}, Family::Ttl}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.record(bus);
    sim.powerOn(3);
    sim.runUntil(60_ns);
    CHECK(sim.value(bus) != Z);  // undriven MOS-only Net: its last driven value
    CHECK(sim.isFloating(bus));
    CHECK(chip.reads.back() == Zero);  // it read the other Driver's release
    sim.runUntil(110_ns);
    CHECK(sim.value(bus) == One);  // driven by the chip 5 ns after EN
    const int before = chip.evaluations;
    CHECK(chip.reads.back() == One);  // its own change woke it
    CHECK(before >= 3);
    sim.runUntil(160_ns);
    CHECK(sim.value(bus) == X);  // MOS against TTL: X, not TTL's 0
    sim.runUntil(190_ns);
    CHECK(sim.value(bus) == One);
    sim.runUntil(300_ns);
    CHECK(sim.value(bus) == One);  // released at 205 ns: holds its charge
    CHECK(sim.isFloating(bus));
}

TEST_CASE("simulator: readBitBefore", "[sim]") {
    Circuit circuit;
    const NetId in = circuit.addNet("IN");
    auto& reader = circuit.add<BeforeReader>("U1");
    circuit.connect(reader, "IN", in);
    AddDrivers(circuit, in, {{Zero, {{10_ns, One}, {20_ns, Zero}}, Family::Mos}});
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(30_ns);
    // Power-on (0), the rise at 10 ns (0 before it), the fall at 20 ns (1 before it).
    CHECK(reader.bits == std::vector<bool>{false, false, true});
}

TEST_CASE("simulator: randomBit in evaluate is seeded", "[sim]") {
    const auto draw = [](std::uint64_t seed) {
        Circuit circuit;
        const NetId clk = circuit.addNet("CLK");
        auto& drawer = circuit.add<RandomDrawer>("U1");
        circuit.connect(drawer, "CLK", clk);
        AddDrivers(circuit, clk, {{Zero, {{10_ns, One}}}});
        circuit.finalize();
        Simulator sim(circuit);
        sim.powerOn(seed);
        sim.runUntil(20_ns);
        return drawer.bits;
    };
    const std::vector<bool> first = draw(1);
    CHECK(first.size() == 16);
    CHECK(draw(1) == first);
    CHECK(draw(2) != first);
}

TEST_CASE("simulator: readBitBefore reads X as a random bit", "[sim]") {
    std::set<bool> seen;
    for (std::uint64_t seed = 1; seed <= 32; ++seed) {
        Circuit circuit;
        const NetId in = circuit.addNet("IN");
        auto& reader = circuit.add<BeforeReader>("U1");
        circuit.connect(reader, "IN", in);
        AddDrivers(circuit, in, {{X, {{10_ns, One}}, Family::Mos}});
        circuit.finalize();
        Simulator sim(circuit);
        sim.powerOn(seed);
        sim.runUntil(20_ns);
        REQUIRE(reader.bits.size() == 2);
        seen.insert(reader.bits[1]);
    }
    CHECK(seen.size() == 2);
}

TEST_CASE("simulator: fell only at a 1 to 0 change", "[sim]") {
    Circuit circuit;
    const NetId net = circuit.addNet("A");
    auto& source = circuit.add<Stimulus>(
        "S", Zero,
        std::vector<Stimulus::Step>{{10_ns, One}, {20_ns, Zero}, {30_ns, X}, {40_ns, Zero}});
    auto& probe = circuit.add<FallProbe>("P");
    circuit.connect(source, Stimulus::kOut, net);
    circuit.connect(probe, "A", net);
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(50_ns);
    CHECK(probe.falls == std::vector<Time>{20_ns});
}
