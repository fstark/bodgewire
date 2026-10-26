#include "sim/primitives.hpp"

#include <array>
#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "sim/circuit.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

LogicValue Eval(GateKind kind, std::initializer_list<LogicValue> inputs) {
    return GateFunction(kind, std::span(inputs.begin(), inputs.size()));
}

LogicValue FromBool(bool b) { return b ? One : Zero; }

}  // namespace

TEST_CASE("gate function: truth tables", "[sim]") {
    for (int a = 0; a < 2; ++a) {
        for (int b = 0; b < 2; ++b) {
            const LogicValue va = FromBool(a != 0);
            const LogicValue vb = FromBool(b != 0);
            CHECK(Eval(GateKind::And, {va, vb}) == FromBool(a && b));
            CHECK(Eval(GateKind::Or, {va, vb}) == FromBool(a || b));
            CHECK(Eval(GateKind::Nand, {va, vb}) == FromBool(!(a && b)));
            CHECK(Eval(GateKind::Nor, {va, vb}) == FromBool(!(a || b)));
            CHECK(Eval(GateKind::Xor, {va, vb}) == FromBool(a != b));
            CHECK(Eval(GateKind::Xnor, {va, vb}) == FromBool(a == b));
            for (int c = 0; c < 2; ++c) {
                const LogicValue vc = FromBool(c != 0);
                CHECK(Eval(GateKind::And, {va, vb, vc}) == FromBool(a && b && c));
                CHECK(Eval(GateKind::Or, {va, vb, vc}) == FromBool(a || b || c));
            }
        }
        CHECK(Eval(GateKind::Not, {FromBool(a != 0)}) == FromBool(a == 0));
        CHECK(Eval(GateKind::Buffer, {FromBool(a != 0)}) == FromBool(a != 0));
    }
}

TEST_CASE("gate function: X handling", "[sim]") {
    CHECK(Eval(GateKind::And, {Zero, X}) == Zero);
    CHECK(Eval(GateKind::And, {One, X}) == X);
    CHECK(Eval(GateKind::Nand, {Zero, X}) == One);
    CHECK(Eval(GateKind::Or, {One, X}) == One);
    CHECK(Eval(GateKind::Or, {Zero, X}) == X);
    CHECK(Eval(GateKind::Xor, {One, X}) == X);
    CHECK(Eval(GateKind::Not, {X}) == X);
    CHECK(Eval(GateKind::Buffer, {X}) == X);
}

TEST_CASE("tri-state: disable gives Z, enable restores", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("a");
    const NetId en = circuit.addNet("en");
    const NetId y = circuit.addNet("y");
    circuit.tie(y, Zero, DriveStrength::Weak);
    auto& sa = circuit.add<Stimulus>("SA", One, std::vector<Stimulus::Step>{});
    auto& sen = circuit.add<Stimulus>("SEN", One,
                                      std::vector<Stimulus::Step>{{100_ns, Zero}, {200_ns, One}});
    auto& buffer = circuit.add<TriStateBuffer>("U1", Delays{5_ns, 6_ns}, 8_ns);
    circuit.connect(sa, Stimulus::kOut, a);
    circuit.connect(sen, Stimulus::kOut, en);
    circuit.connect(buffer, "A", a);
    circuit.connect(buffer, "EN", en);
    circuit.connect(buffer, "Y", y);
    circuit.finalize();

    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(50_ns);
    CHECK(sim.value(y) == One);
    sim.record(y);
    sim.runUntil(300_ns);

    const auto samples = sim.recorder().samples(y);
    REQUIRE(samples.size() == 3);
    CHECK((samples[1] == Sample{108_ns, Zero}));  // released, weak pull-down
    CHECK((samples[2] == Sample{208_ns, One}));   // re-enabled uses the toggle delay
}

namespace {

// Stimuli on D and CLK, DFlipFlop with 5 ns clock-to-Q.
struct DffRun {
    Circuit circuit;
    NetId q{};
    NetId qn{};

    DffRun(std::vector<Stimulus::Step> d, std::vector<Stimulus::Step> clk) {
        const NetId dNet = circuit.addNet("d");
        const NetId clkNet = circuit.addNet("clk");
        q = circuit.addNet("q");
        qn = circuit.addNet("qn");
        auto& sd = circuit.add<Stimulus>("SD", Zero, std::move(d));
        auto& sclk = circuit.add<Stimulus>("SCLK", Zero, std::move(clk));
        auto& ff = circuit.add<DFlipFlop>("U1", Delays{5_ns, 5_ns});
        circuit.connect(sd, Stimulus::kOut, dNet);
        circuit.connect(sclk, Stimulus::kOut, clkNet);
        circuit.connect(ff, "D", dNet);
        circuit.connect(ff, "CLK", clkNet);
        circuit.connect(ff, "Q", q);
        circuit.connect(ff, "Q_N", qn);
        circuit.finalize();
    }
};

}  // namespace

TEST_CASE("dff: captures D on rising edge only", "[sim]") {
    DffRun r({{50_ns, One}, {120_ns, Zero}},
             {{100_ns, One}, {150_ns, Zero}, {200_ns, One}});
    Simulator sim(r.circuit);
    sim.powerOn(1);
    sim.runUntil(110_ns);
    CHECK(sim.value(r.q) == One);
    CHECK(sim.value(r.qn) == Zero);
    sim.runUntil(190_ns);  // D changed at 120, CLK fell at 150: no capture
    CHECK(sim.value(r.q) == One);
    sim.runUntil(210_ns);
    CHECK(sim.value(r.q) == Zero);
    CHECK(sim.value(r.qn) == One);
}

TEST_CASE("dff: samples D as before a same-timestamp clock edge", "[sim]") {
    DffRun r({{100_ns, One}, {200_ns, Zero}},
             {{50_ns, One}, {75_ns, Zero}, {100_ns, One}, {150_ns, Zero}, {200_ns, One}});
    Simulator sim(r.circuit);
    sim.powerOn(1);
    sim.runUntil(60_ns);
    CHECK(sim.value(r.q) == Zero);
    sim.runUntil(110_ns);  // D rose with CLK at 100 ns: old D sampled
    CHECK(sim.value(r.q) == Zero);
    sim.runUntil(190_ns);
    CHECK(sim.value(r.q) == Zero);
    sim.runUntil(210_ns);  // D fell with CLK at 200 ns: old D (1) sampled
    CHECK(sim.value(r.q) == One);
}

TEST_CASE("dff: power-on Q and Q_N complementary", "[sim]") {
    DffRun r({}, {});
    Simulator sim(r.circuit);
    std::set<LogicValue> seen;
    for (std::uint64_t seed = 1; seed <= 16; ++seed) {
        sim.powerOn(seed);
        CHECK(sim.value(r.q) != X);
        CHECK(sim.value(r.qn) == Not(sim.value(r.q)));
        seen.insert(sim.value(r.q));
    }
    CHECK(seen.size() == 2);
}

TEST_CASE("clock: period and duty", "[sim]") {
    Circuit circuit;
    const NetId clk = circuit.addNet("clk");
    auto& source = circuit.add<ClockSource>("X1", 1001);
    circuit.connect(source, ClockSource::kOut, clk);
    circuit.finalize();
    Simulator sim(circuit);
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        sim.powerOn(seed);
        sim.record(clk);
        sim.runFor(10'000);
        const auto samples = sim.recorder().samples(clk);
        REQUIRE(samples.size() > 10);
        for (std::size_t i = 1; i + 1 < samples.size(); ++i) {
            const Time held = samples[i + 1].time - samples[i].time;
            CHECK(held == (samples[i].value == One ? 500 : 501));
        }
    }
}

TEST_CASE("ring oscillator: period is 2 x 3 x delay", "[sim]") {
    Circuit circuit;
    const NetId en = circuit.addNet("en");
    const NetId n1 = circuit.addNet("n1");
    const NetId n2 = circuit.addNet("n2");
    const NetId n3 = circuit.addNet("n3");
    auto& enable = circuit.add<Stimulus>("SEN", Zero, std::vector<Stimulus::Step>{{100_ns, One}});
    auto& nand = circuit.add<Gate>("U1", GateKind::Nand, 2, Delays{5_ns, 5_ns});
    auto& inv1 = circuit.add<Gate>("U2", GateKind::Not, 1, Delays{5_ns, 5_ns});
    auto& inv2 = circuit.add<Gate>("U3", GateKind::Not, 1, Delays{5_ns, 5_ns});
    circuit.connect(enable, Stimulus::kOut, en);
    circuit.connect(nand, "I0", en);
    circuit.connect(nand, "I1", n3);
    circuit.connect(nand, nand.output(), n1);
    circuit.connect(inv1, "I0", n1);
    circuit.connect(inv1, inv1.output(), n2);
    circuit.connect(inv2, "I0", n2);
    circuit.connect(inv2, inv2.output(), n3);
    circuit.finalize();

    Simulator sim(circuit);
    sim.powerOn(1);
    sim.runUntil(1_us);
    sim.record(n3);
    sim.runUntil(2_us);
    const auto samples = sim.recorder().samples(n3);
    REQUIRE(samples.size() > 10);
    for (std::size_t i = 1; i + 1 < samples.size(); ++i) {
        CHECK(samples[i + 1].time - samples[i].time == 15_ns);
    }
}

TEST_CASE("ripple counter: counts", "[sim]") {
    Circuit circuit;
    const NetId clk = circuit.addNet("clk");
    auto& source = circuit.add<ClockSource>("X1", 1_us);
    circuit.connect(source, ClockSource::kOut, clk);
    std::vector<NetId> q;
    NetId stageClock = clk;
    for (int i = 0; i < 4; ++i) {
        const std::string index = std::to_string(i);
        q.push_back(circuit.addNet("q" + index));
        const NetId qn = circuit.addNet("qn" + index);
        auto& ff = circuit.add<DFlipFlop>("U" + index, Delays{5_ns, 5_ns});
        circuit.connect(ff, "CLK", stageClock);
        circuit.connect(ff, "D", qn);
        circuit.connect(ff, "Q", q.back());
        circuit.connect(ff, "Q_N", qn);
        stageClock = qn;
    }
    circuit.finalize();

    const auto count = [&](const Simulator& sim) {
        unsigned value = 0;
        for (std::size_t i = 0; i < q.size(); ++i) {
            value |= (sim.value(q[i]) == One ? 1u : 0u) << i;
        }
        return value;
    };

    Simulator sim(circuit);
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        sim.powerOn(seed);
        sim.runUntil(20_ns);  // first clock edge is at 500 ns or 1 us
        const unsigned start = count(sim);
        constexpr unsigned kCycles = 21;
        sim.runUntil(20_ns + kCycles * 1_us + 100_ns);  // exactly kCycles rising edges, settled
        CHECK(count(sim) == (start + kCycles) % 16);
    }
}
