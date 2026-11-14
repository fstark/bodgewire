// Switched Pins (P5i): Supply drive, external drives, waking on the other drives only.

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

// One Switched Pin per Net; while `closed`, joins them all as the switch network does.
class Joiner final : public Component {
public:
    explicit Joiner(std::size_t pins) : Component("joiner", Specs(pins)) {}
    bool closed = false;
    int evaluations = 0;
    Time lastAt = -1;

    void evaluate(EvalContext& ctx) override {
        ++evaluations;
        lastAt = ctx.now();
        const auto count = static_cast<PinIndex>(pins().size());
        LogicValue value = Z;
        if (closed) {
            Resolver r;
            for (PinIndex p = 0; p < count; ++p) {
                r.merge(ctx.external(p));
            }
            const Resolution res = r.finish(false, Zero);
            value = res.floating ? Z : res.value;
        }
        for (PinIndex p = 0; p < count; ++p) {
            ctx.drive(p, value, 1);
        }
    }

private:
    static std::vector<PinSpec> Specs(std::size_t pins) {
        std::vector<PinSpec> specs;
        for (std::size_t p = 0; p < pins; ++p) {
            specs.push_back({"P" + std::to_string(p), PinKind::Switched, Family::Ttl});
        }
        return specs;
    }
};

// A TTL input on `net`, so a floating Net reads 1.
class Load final : public Component {
public:
    explicit Load(std::string name)
        : Component(std::move(name), {{"A", PinKind::Input, Family::Ttl}}) {}
    void evaluate(EvalContext&) override {}
};

struct Bench {
    Circuit circuit;
    NetId a = circuit.addNet("A");
    NetId b = circuit.addNet("B");
    Joiner& joiner = circuit.add<Joiner>(2);

    Bench() {
        circuit.connect(joiner, 0, a);
        circuit.connect(joiner, 1, b);
    }
    Stimulus& drive(NetId net, LogicValue initial, std::vector<Stimulus::Step> steps = {},
                    Family family = Family::Ttl) {
        auto& s = circuit.add<Stimulus>("drv" + std::to_string(count++), initial, std::move(steps),
                                        family);
        circuit.connect(s, Stimulus::kOut, net);
        return s;
    }
    void close(Simulator& sim, bool closed) {
        joiner.closed = closed;
        sim.wake(joiner.id(), 1);
        sim.runFor(10);
    }
    int count = 0;
};

}  // namespace

TEST_CASE("switched: merge gives what add gives", "[sim]") {
    const std::vector<Drive> drives{
        {One, DriveStrength::Weak, Family::Ttl},    {Zero, DriveStrength::Strong, Family::Ttl},
        {One, DriveStrength::Strong, Family::Mos},  {X, DriveStrength::Strong, Family::Ttl},
        {Zero, DriveStrength::Supply, Family::Ttl}, {Z, DriveStrength::None, Family::Ttl},
        {Zero, DriveStrength::Weak, Family::Ttl},
    };
    // Every split of every pair and triple: add all into one, or add in two halves and merge.
    for (std::size_t i = 0; i < drives.size(); ++i) {
        for (std::size_t j = 0; j < drives.size(); ++j) {
            for (std::size_t k = 0; k < drives.size(); ++k) {
                Resolver all, left, right;
                all.add(drives[i]);
                all.add(drives[j]);
                all.add(drives[k]);
                left.add(drives[i]);
                right.add(drives[j]);
                right.add(drives[k]);
                left.merge(right);
                CHECK(left.finish(true, Zero).value == all.finish(true, Zero).value);
                CHECK(left.finish(true, Zero).floating == all.finish(true, Zero).floating);
                CHECK(left.strength() == all.strength());
            }
        }
    }
}

TEST_CASE("switched: a Supply drive, Z at power-on", "[sim]") {
    CHECK(DriveOf(PinKind::Switched, Family::Ttl, One).strength == DriveStrength::Supply);
    CHECK(DriveOf(PinKind::Switched, Family::Ttl, Z).strength == DriveStrength::None);
    CHECK(IsDriver(PinKind::Switched));
    CHECK_FALSE(IsReader(PinKind::Switched));

    Bench bench;
    bench.drive(bench.a, One);
    bench.circuit.add<Load>("load");
    bench.circuit.connect(*bench.circuit.components().back(), 0, bench.b);
    bench.circuit.finalize();
    Simulator sim(bench.circuit);
    sim.powerOn(7);
    sim.runFor(10);
    CHECK(sim.value(bench.a) == One);
    CHECK(sim.isFloating(bench.b));  // open: the joiner drives nothing
}

TEST_CASE("switched: closed, the Nets take the strongest drive of the pair", "[sim]") {
    SECTION("a pull-up reaches the other Net") {
        Bench bench;
        bench.circuit.tie(bench.a, One, DriveStrength::Weak);
        bench.circuit.finalize();
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        bench.close(sim, true);
        CHECK(sim.value(bench.b) == One);
        CHECK_FALSE(sim.isFloating(bench.b));
        bench.close(sim, false);
        CHECK(sim.isFloating(bench.b));
    }
    SECTION("a rail beats a TTL output") {
        Bench bench;
        bench.circuit.tie(bench.a, Zero, DriveStrength::Supply);
        bench.drive(bench.b, One);
        bench.circuit.finalize();
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        bench.close(sim, true);
        CHECK(sim.value(bench.a) == Zero);
        CHECK(sim.value(bench.b) == Zero);
        bench.close(sim, false);
        CHECK(sim.value(bench.b) == One);
    }
    SECTION("two TTL outputs fight to 0") {
        Bench bench;
        bench.drive(bench.a, One);
        bench.drive(bench.b, Zero);
        bench.circuit.finalize();
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        bench.close(sim, true);
        CHECK(sim.value(bench.a) == Zero);
        CHECK(sim.value(bench.b) == Zero);
    }
    SECTION("a MOS fight and an X give X") {
        Bench bench;
        bench.drive(bench.a, One, {}, Family::Mos);
        bench.drive(bench.b, Zero);
        bench.circuit.finalize();
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        bench.close(sim, true);
        CHECK(sim.value(bench.a) == X);
        CHECK(sim.value(bench.b) == X);
    }
    SECTION("nothing driving: each Net floats alone") {
        Bench bench;
        bench.circuit.finalize();
        Simulator sim(bench.circuit);
        sim.powerOn(1);
        bench.close(sim, true);
        CHECK(sim.isFloating(bench.a));
        CHECK(sim.isFloating(bench.b));
    }
}

TEST_CASE("switched: woken by the other drives only, never by its own", "[sim]") {
    Bench bench;
    bench.drive(bench.a, Zero, {{1000, One}, {2000, Zero}});
    bench.circuit.tie(bench.b, One, DriveStrength::Weak);
    bench.circuit.finalize();
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    bench.close(sim, true);
    CHECK(sim.value(bench.b) == Zero);
    const int settled = bench.joiner.evaluations;
    sim.runUntil(999);
    CHECK(bench.joiner.evaluations == settled);  // its own drives woke nothing

    sim.runUntil(1000);
    // At 1000 A's driver goes 1: the joiner still forces 0, so A's value holds, but its
    // drives changed: woken now, and both Nets move 1 ps later.
    CHECK(bench.joiner.evaluations == settled + 1);
    CHECK(bench.joiner.lastAt == 1000);
    CHECK(sim.value(bench.a) == Zero);
    sim.runUntil(1001);
    CHECK(sim.value(bench.a) == One);
    CHECK(sim.value(bench.b) == One);
    sim.runUntil(1999);
    CHECK(bench.joiner.evaluations == settled + 1);
    sim.runUntil(2001);
    CHECK(bench.joiner.evaluations == settled + 2);
    CHECK(sim.value(bench.b) == Zero);
}

TEST_CASE("switched: a chain of three Nets", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("A"), b = circuit.addNet("B"), c = circuit.addNet("C");
    Joiner& joiner = circuit.add<Joiner>(3);
    circuit.connect(joiner, 0, a);
    circuit.connect(joiner, 1, b);
    circuit.connect(joiner, 2, c);
    circuit.tie(c, Zero, DriveStrength::Supply);
    circuit.tie(a, One, DriveStrength::Weak);
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    joiner.closed = true;
    sim.wake(joiner.id(), 1);
    sim.runFor(10);
    CHECK(sim.value(a) == Zero);
    CHECK(sim.value(b) == Zero);
}

TEST_CASE("switched: one per Net", "[sim]") {
    Circuit circuit;
    const NetId a = circuit.addNet("A");
    Joiner& joiner = circuit.add<Joiner>(2);
    circuit.connect(joiner, 0, a);
    circuit.connect(joiner, 1, a);
    CHECK_THROWS(circuit.finalize());
}
