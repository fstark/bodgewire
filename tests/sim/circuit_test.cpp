#include "sim/circuit.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

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

class PoweredPart : public Component {
public:
    explicit PoweredPart(std::string name)
        : Component(std::move(name), {{"VCC", PinKind::Power, Family::Ttl},
                                      {"NC2", PinKind::NoConnect, Family::Ttl}}) {}
    void evaluate(EvalContext& /*ctx*/) override {}
};

std::vector<PinSpec> InputPins(std::size_t count) {
    std::vector<PinSpec> pins;
    for (std::size_t i = 0; i < count; ++i) {
        pins.push_back({"I" + std::to_string(i), PinKind::Input, Family::Ttl});
    }
    return pins;
}

class ManyPins : public Component {
public:
    ManyPins(std::string name, std::size_t count) : Component(std::move(name), InputPins(count)) {}
    void evaluate(EvalContext& /*ctx*/) override {}
};

class EdgePins : public Component {
public:
    explicit EdgePins(std::string name)
        : Component(std::move(name), {{"CLK1", PinKind::Input, Family::Ttl, true},
                                      {"D", PinKind::Input, Family::Ttl},
                                      {"CLK2", PinKind::Input, Family::Ttl, true},
                                      {"CLK3", PinKind::Input, Family::Ttl, true}}) {}
    void evaluate(EvalContext& /*ctx*/) override {}
};

}  // namespace

TEST_CASE("circuit: power and no-connect pins are neither drivers nor readers", "[sim]") {
    Circuit circuit;
    PoweredPart& u1 = circuit.add<PoweredPart>("U1");
    Dummy& u2 = circuit.add<Dummy>("U2");
    const NetId n = circuit.addNet("n");
    circuit.connect(u1, "VCC", n);
    circuit.connect(u1, "NC2", n);
    circuit.connect(u2, "A", n);
    circuit.finalize();

    CHECK(circuit.drivers(n).empty());
    CHECK(circuit.hasTtlInput(n));
    const auto fanOut = circuit.fanOut(n);
    REQUIRE(fanOut.size() == 1);
    CHECK(fanOut[0].component == u2.id());
    CHECK(fanOut[0].mask == 0b1);
}

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

TEST_CASE("circuit: more than 64 pins rejected", "[sim]") {
    Circuit circuit;
    CHECK_NOTHROW(circuit.add<ManyPins>("U1", 64));
    CHECK_THROWS_AS(circuit.add<ManyPins>("U2", 65), std::logic_error);
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
    circuit.connect(u1, "Y", n);  // output only: not a reader
    circuit.connect(u2, "A", n);  // two inputs of one component
    circuit.connect(u2, "B", n);
    circuit.finalize();

    const auto drivers = circuit.drivers(n);
    REQUIRE(drivers.size() == 1);
    CHECK(drivers[0] == Pin(u1, 2));

    const auto fanOut = circuit.fanOut(n);
    REQUIRE(fanOut.size() == 1);
    CHECK(fanOut[0].component == u2.id());
    CHECK(fanOut[0].mask == 0b11);
}

TEST_CASE("circuit: fan-out merges pins of one component", "[sim]") {
    Circuit circuit;
    EdgePins& u1 = circuit.add<EdgePins>("U1");
    const NetId mixed = circuit.addNet("mixed");
    const NetId edges = circuit.addNet("edges");
    circuit.connect(u1, "CLK1", mixed);
    circuit.connect(u1, "D", mixed);
    circuit.connect(u1, "CLK2", edges);
    circuit.connect(u1, "CLK3", edges);
    circuit.finalize();

    const auto mixedFanOut = circuit.fanOut(mixed);
    REQUIRE(mixedFanOut.size() == 1);
    CHECK(mixedFanOut[0].mask == 0b0011);
    CHECK_FALSE(mixedFanOut[0].risingEdgeOnly);

    const auto edgesFanOut = circuit.fanOut(edges);
    REQUIRE(edgesFanOut.size() == 1);
    CHECK(edgesFanOut[0].mask == 0b1100);
    CHECK(edgesFanOut[0].risingEdgeOnly);
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

TEST_CASE("circuit: single driver only for one untied totem-pole output", "[sim]") {
    Circuit circuit;
    Dummy& u1 = circuit.add<Dummy>("U1");
    Dummy& u2 = circuit.add<Dummy>("U2");
    Dummy& u3 = circuit.add<Dummy>("U3");
    Dummy& u4 = circuit.add<Dummy>("U4");
    const NetId single = circuit.addNet("single");
    const NetId shared = circuit.addNet("shared");
    const NetId pulled = circuit.addNet("pulled");
    circuit.connect(u1, "Y", single);
    circuit.connect(u1, "A", shared);
    circuit.connect(u2, "Y", shared);
    circuit.connect(u3, "Y", shared);
    circuit.connect(u4, "Y", pulled);
    circuit.connect(u2, "A", pulled);
    circuit.tie(pulled, LogicValue::One, DriveStrength::Weak);
    circuit.finalize();

    CHECK(circuit.singleDriver(single) == Pin(u1, 2));
    CHECK(circuit.singleDriver(shared) == Circuit::kNoPin);
    CHECK(circuit.singleDriver(pulled) == Circuit::kNoPin);
}
