#include "parts/led.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

TEST_CASE("led: catalogue entry", "[parts]") {
    const PartInfo* info = FindPart("LED");
    REQUIRE(info != nullptr);
    CHECK(info->kind == PartKind::Led);
    CHECK(info->package == "LED-5mm");
    Circuit circuit;
    const std::vector<PartParam> params{{"colour", "red"}};
    const auto created = info->create(circuit, "D1", params);
    REQUIRE_FALSE(created.has_value());
    CHECK(created.error() == "LED takes no parameters");
}

TEST_CASE("led: pins are passive loads", "[parts]") {
    Circuit circuit;
    const auto created = FindPart("LED")->create(circuit, "D1", {});
    REQUIRE(created.has_value());
    const auto pins = (*created)->pins();
    REQUIRE(pins.size() == 2);
    CHECK(pins[0].name == "K");
    CHECK(pins[1].name == "A");
    for (const PinSpec& pin : pins) {
        CHECK(pin.kind == PinKind::Input);
        CHECK(pin.family == Family::Mos);
    }
}

TEST_CASE("led: a Net with only LED pins stays floating", "[parts]") {
    Circuit circuit;
    const NetId k = circuit.addNet("K");
    const NetId a = circuit.addNet("A");
    Component& d1 = *FindPart("LED")->create(circuit, "D1", {}).value();
    circuit.connect(d1, "K", k);
    circuit.connect(d1, "A", a);
    circuit.finalize();
    Simulator sim(circuit);
    sim.powerOn(1);
    sim.record(k);
    sim.runFor(10_us);
    CHECK(sim.isFloating(k));
    const auto samples = sim.recorder().samples(k);
    REQUIRE_FALSE(samples.empty());
    CHECK(samples.front().floating);
}
