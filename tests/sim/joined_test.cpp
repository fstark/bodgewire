// Joined Pins (P5u): a Weak drive, released at power-on, woken by the other drives only; one
// network Pin a Net, Switched or Joined.

#include <cstddef>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;

namespace {

using enum LogicValue;

// One Joined Pin on each of two Nets: drives each Net with the other's external value, as a
// series resistor between them would when one side alone is driven.
class Copier final : public Component {
public:
    Copier() : Component("copier", Specs()) {}
    int evaluations = 0;
    Time lastAt = -1;

    void evaluate(EvalContext& ctx) override {
        ++evaluations;
        lastAt = ctx.now();
        for (PinIndex p = 0; p < 2; ++p) {
            const Resolver& other = ctx.external(1 - p);
            const bool own = ctx.external(p).strength() != DriveStrength::None;
            const bool there = other.strength() != DriveStrength::None;
            ctx.drive(p, own || !there ? Z : other.finish(false, Zero).value, 1);
        }
    }

private:
    static std::vector<PinSpec> Specs() {
        return {{"P0", PinKind::Joined, Family::Ttl}, {"P1", PinKind::Joined, Family::Ttl}};
    }
};

struct Bench {
    Circuit circuit;
    NetId a = circuit.addNet("A");
    NetId b = circuit.addNet("B");
    Copier& copier = circuit.add<Copier>();

    Bench() {
        circuit.connect(copier, 0, a);
        circuit.connect(copier, 1, b);
    }
    Stimulus& drive(NetId net, LogicValue initial, std::vector<Stimulus::Step> steps = {}) {
        auto& s = circuit.add<Stimulus>("drv" + std::to_string(count++), initial, std::move(steps),
                                        Family::Ttl);
        circuit.connect(s, Stimulus::kOut, net);
        return s;
    }
    int count = 0;
};

}  // namespace

TEST_CASE("joined: a Weak drive, a network Pin", "[sim]") {
    CHECK(DriveOf(PinKind::Joined, Family::Ttl, One).strength == DriveStrength::Weak);
    CHECK(DriveOf(PinKind::Joined, Family::Ttl, Zero).strength == DriveStrength::Weak);
    CHECK(DriveOf(PinKind::Joined, Family::Ttl, Z).strength == DriveStrength::None);
    CHECK(IsDriver(PinKind::Joined));
    CHECK_FALSE(IsReader(PinKind::Joined));
    CHECK(IsNetworkPin(PinKind::Joined));
    CHECK(IsNetworkPin(PinKind::Switched));
    CHECK_FALSE(IsNetworkPin(PinKind::PullUp));
}

TEST_CASE("joined: released at power-on, then the other Net's value, weakly", "[sim]") {
    Bench bench;
    bench.drive(bench.a, One);
    bench.circuit.finalize();
    Simulator sim(bench.circuit);
    sim.powerOn(3);
    CHECK(sim.isFloating(bench.b));  // released: the power-on evaluation drives it 1 ps later
    sim.runFor(10);
    CHECK(sim.value(bench.b) == One);
    CHECK_FALSE(sim.isFloating(bench.b));
}

TEST_CASE("joined: an output beats the Weak drive on its own Net", "[sim]") {
    Bench bench;
    bench.drive(bench.a, One);
    bench.drive(bench.b, Zero);
    bench.circuit.finalize();
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runFor(10);
    CHECK(sim.value(bench.a) == One);
    CHECK(sim.value(bench.b) == Zero);
}

TEST_CASE("joined: woken by the other drives only, never by its own", "[sim]") {
    Bench bench;
    bench.drive(bench.a, Zero, {{1000, One}, {2000, Zero}});
    bench.circuit.finalize();
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runFor(10);
    CHECK(sim.value(bench.b) == Zero);
    const int settled = bench.copier.evaluations;
    sim.runUntil(999);
    CHECK(bench.copier.evaluations == settled);  // its drive of B woke nothing

    sim.runUntil(1000);
    CHECK(bench.copier.evaluations == settled + 1);
    CHECK(bench.copier.lastAt == 1000);
    CHECK(sim.value(bench.b) == Zero);
    sim.runUntil(1001);
    CHECK(sim.value(bench.b) == One);
    sim.runUntil(1999);
    CHECK(bench.copier.evaluations == settled + 1);
    sim.runUntil(2001);
    CHECK(bench.copier.evaluations == settled + 2);
    CHECK(sim.value(bench.b) == Zero);
}

TEST_CASE("joined: one network Pin a Net", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("A");
    Copier& copier = circuit.add<Copier>();
    circuit.connect(copier, 0, a);
    circuit.connect(copier, 1, a);
    CHECK_THROWS(circuit.finalize());
}
