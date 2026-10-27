#include "parts/connector.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "parts/catalogue.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

std::vector<std::string> PinNames(std::string_view part) {
    Circuit circuit;
    const PartInfo* info = FindPart(part);
    REQUIRE(info != nullptr);
    CHECK(info->kind == PartKind::Connector);
    const auto created = info->create(circuit, "J1", {});
    REQUIRE(created.has_value());
    std::vector<std::string> names;
    for (const PinSpec& pin : (*created)->pins()) {
        names.push_back(pin.name);
        CHECK(pin.kind ==
              (pin.name == "VCC" || pin.name == "GND" ? PinKind::Power : PinKind::Input));
    }
    return names;
}

}  // namespace

TEST_CASE("connector: POWER and VIDEO pins", "[parts]") {
    CHECK(PinNames("POWER") == std::vector<std::string>{"VCC", "GND"});
    CHECK(PinNames("VIDEO") == std::vector<std::string>{"VIDEO", "SYNC_N", "GND"});
    CHECK(FindPart("POWER")->package == "KK-2");
    CHECK(FindPart("VIDEO")->package == "KK-3");
}

TEST_CASE("connector: parameters rejected", "[parts]") {
    Circuit circuit;
    const std::vector<PartParam> params{{"pins", "4"}};
    const auto created = FindPart("VIDEO")->create(circuit, "J2", params);
    REQUIRE_FALSE(created.has_value());
    CHECK(created.error() == "VIDEO takes no parameters");
}

TEST_CASE("connector: VIDEO is a passive load", "[parts]") {
    const auto loaded = LoadNetlist("part X1 OSC freq=1MHz\npart J2 VIDEO\n"
                                    "net CLK X1.OUT J2.VIDEO\nnet GND J2.SYNC_N\n",
                                    "t.bwn");
    REQUIRE(loaded.has_value());
    CHECK((*loaded)->warnings.empty());
    Simulator sim((*loaded)->circuit);
    sim.powerOn(1);
    sim.runFor(10_us);
    CHECK(sim.stats().appliedEvents >= 19);
    CHECK(sim.value((*loaded)->nets.at("GND")) == LogicValue::Zero);
}

TEST_CASE("connector: POWER pins are on VCC and GND without net lines", "[parts]") {
    const auto loaded = LoadNetlist("part J1 POWER\n", "t.bwn");
    REQUIRE(loaded.has_value());
    std::vector<std::string> connections;
    for (const Connection& c : (*loaded)->connections) {
        connections.push_back(c.reference + "." + std::to_string(c.pin + 1) + " " + c.net);
    }
    CHECK(connections == std::vector<std::string>{"J1.1 VCC", "J1.2 GND"});
}
