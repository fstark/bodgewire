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

TEST_CASE("connector: the PET's edge connectors and cassette port are sampled loads", "[parts]") {
    const auto pins = [](std::string_view part) {
        Circuit circuit;
        const PartInfo* info = FindPart(part);
        REQUIRE(info != nullptr);
        CHECK(info->kind == PartKind::Connector);
        const auto created = info->create(circuit, "J1", {});
        REQUIRE(created.has_value());
        std::vector<std::string> names;
        for (const PinSpec& pin : (*created)->pins()) {
            names.push_back(pin.name);
            CHECK(pin.kind == PinKind::Input);
            CHECK(pin.family == Family::Mos);
            CHECK(pin.sampled);
        }
        return names;
    };
    const auto ieee = pins("IEEE488");
    REQUIRE(ieee.size() == 24);
    // Pad 2n-1 is the PET's pin n, pad 2k its k-th letter: DIO5 (A) faces DIO1 (1).
    CHECK(ieee[0] == "DIO1");
    CHECK(ieee[1] == "DIO5");
    CHECK(ieee[8] == "EOI_N");
    CHECK(ieee[20] == "ATN_N");
    const auto user = pins("USERPORT");
    REQUIRE(user.size() == 24);
    CHECK(user[8] == "DIAG");   // pin 5
    CHECK(user[5] == "PA0");    // C
    CHECK(user[21] == "CB2");   // M
    CHECK(user[2] == "VIDEO");  // pin 2
    CHECK(pins("CASSETTE") ==
          std::vector<std::string>{"GND", "VCC", "MOTOR", "READ", "WRITE", "SENSE"});
    CHECK(FindPart("IEEE488")->package == "HDR-2x12");
    CHECK(FindPart("USERPORT")->package == "HDR-2x12");
    CHECK(FindPart("CASSETTE")->package == "HDR-1x06");
    CHECK_FALSE(ConnectorPowersDevice("CASSETTE"));
}

TEST_CASE("connector: a cassette port on the rails gives no warning", "[parts]") {
    const auto loaded = LoadNetlist("part J1 POWER\npart J3 CASSETTE\nnet VCC J3.VCC\n"
                                    "net GND J3.GND\n",
                                    "t.bwn");
    REQUIRE(loaded.has_value());
    CHECK((*loaded)->warnings.empty());
}
