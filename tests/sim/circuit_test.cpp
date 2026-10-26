#include "sim/circuit.hpp"

#include <algorithm>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

class Dummy : public Component {
public:
    explicit Dummy(std::string name)
        : Component(std::move(name), {{"A", PinKind::Input, Family::Ttl},
                                      {"B", PinKind::Input, Family::Mos},
                                      {"Y", PinKind::Output, Family::Ttl}}) {}
    void evaluate(EvalContext& /*ctx*/) override {}
};

PinId Pin(const Component& component, PinIndex index) {
    return PinId{ToIndex(component.firstPin()) + index};
}

}  // namespace

TEST_CASE("circuit: pin ids are contiguous per component", "[sim]") {
    Circuit circuit;
    Dummy& u1 = circuit.add<Dummy>("U1");
    Dummy& u2 = circuit.add<Dummy>("U2");
    CHECK(ToIndex(u1.id()) == 0);
    CHECK(ToIndex(u2.id()) == 1);
    CHECK(ToIndex(u1.firstPin()) == 0);
    CHECK(ToIndex(u2.firstPin()) == 3);
    CHECK(circuit.pinCount() == 6);
    CHECK(circuit.pinInfo(Pin(u2, 2)).owner == u2.id());
    CHECK(circuit.pinInfo(Pin(u2, 2)).kind == PinKind::Output);
}

TEST_CASE("circuit: connect twice throws", "[sim]") {
    Circuit circuit;
    Dummy& u1 = circuit.add<Dummy>("U1");
    const NetId a = circuit.addNet("a");
    const NetId b = circuit.addNet("b");
    circuit.connect(u1, "A", a);
    CHECK_THROWS_AS(circuit.connect(u1, "A", b), std::logic_error);
}

TEST_CASE("circuit: unknown pin name throws", "[sim]") {
    Circuit circuit;
    Dummy& u1 = circuit.add<Dummy>("U1");
    const NetId a = circuit.addNet("a");
    CHECK_THROWS_AS(circuit.connect(u1, "Q", a), std::logic_error);
}

TEST_CASE("circuit: edit after finalize throws", "[sim]") {
    Circuit circuit;
    Dummy& u1 = circuit.add<Dummy>("U1");
    const NetId a = circuit.addNet("a");
    circuit.finalize();
    CHECK(circuit.isFinalized());
    CHECK_THROWS_AS(circuit.addNet("b"), std::logic_error);
    CHECK_THROWS_AS(circuit.add<Dummy>("U2"), std::logic_error);
    CHECK_THROWS_AS(circuit.connect(u1, "A", a), std::logic_error);
    CHECK_THROWS_AS(circuit.tie(a, LogicValue::One, DriveStrength::Supply), std::logic_error);
    CHECK_THROWS_AS(circuit.finalize(), std::logic_error);
}

TEST_CASE("circuit: unconnected pins get private nets", "[sim]") {
    Circuit circuit;
    Dummy& u1 = circuit.add<Dummy>("U1");
    const NetId b = circuit.addNet("b");
    circuit.connect(u1, "B", b);
    circuit.connect(u1, "Y", b);
    circuit.finalize();
    CHECK(circuit.netCount() == 2);
    const NetId a = circuit.netOf(Pin(u1, 0));
    CHECK(a != b);
    CHECK(circuit.netName(a) == "U1.A");
}

TEST_CASE("circuit: drivers and readers", "[sim]") {
    Circuit circuit;
    Dummy& u1 = circuit.add<Dummy>("U1");
    Dummy& u2 = circuit.add<Dummy>("U2");
    const NetId n = circuit.addNet("n");
    circuit.connect(u1, "Y", n);   // output only: not a reader
    circuit.connect(u2, "A", n);   // two inputs of one component
    circuit.connect(u2, "B", n);
    circuit.finalize();

    const auto drivers = circuit.drivers(n);
    REQUIRE(drivers.size() == 1);
    CHECK(drivers[0] == Pin(u1, 2));

    const auto readers = circuit.readers(n);
    REQUIRE(readers.size() == 1);
    CHECK(readers[0] == u2.id());
}

TEST_CASE("circuit: TTL input flag", "[sim]") {
    Circuit circuit;
    Dummy& u1 = circuit.add<Dummy>("U1");
    const NetId a = circuit.addNet("a");
    const NetId b = circuit.addNet("b");
    const NetId y = circuit.addNet("y");
    circuit.connect(u1, "A", a);
    circuit.connect(u1, "B", b);
    circuit.connect(u1, "Y", y);
    circuit.finalize();
    CHECK(circuit.hasTtlInput(a));
    CHECK_FALSE(circuit.hasTtlInput(b));
    CHECK_FALSE(circuit.hasTtlInput(y));
}

TEST_CASE("circuit: ties combine", "[sim]") {
    Circuit circuit;
    const NetId untied = circuit.addNet("untied");
    const NetId supply = circuit.addNet("supply");
    const NetId clash = circuit.addNet("clash");
    circuit.tie(supply, LogicValue::One, DriveStrength::Weak);
    circuit.tie(supply, LogicValue::Zero, DriveStrength::Supply);
    circuit.tie(supply, LogicValue::One, DriveStrength::Weak);
    circuit.tie(clash, LogicValue::One, DriveStrength::Weak);
    circuit.tie(clash, LogicValue::Zero, DriveStrength::Weak);
    circuit.finalize();

    CHECK(circuit.tieOf(untied).strength == DriveStrength::None);
    CHECK(circuit.tieOf(supply).strength == DriveStrength::Supply);
    CHECK(circuit.tieOf(supply).value == LogicValue::Zero);
    CHECK(circuit.tieOf(supply).family == Family::Ttl);
    CHECK(circuit.tieOf(clash).strength == DriveStrength::Weak);
    CHECK(circuit.tieOf(clash).value == LogicValue::X);
}
