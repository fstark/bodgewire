#include "sim/primitives.hpp"

#include <array>
#include <cstdint>
#include <span>

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
