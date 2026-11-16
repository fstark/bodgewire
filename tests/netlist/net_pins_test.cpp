#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "netlist/net_pins.hpp"

using namespace bw;

namespace {

std::unique_ptr<LoadedCircuit> Load(const std::string& name) {
    auto result = LoadNetlistFile(test::NetlistPath(name));
    REQUIRE(result.has_value());
    return std::move(*result);
}

std::vector<std::string> Texts(const std::vector<NetPin>& pins) {
    std::vector<std::string> texts;
    for (const NetPin& pin : pins) {
        texts.push_back(FormatNetPin(pin));
    }
    return texts;
}

bool Contains(const std::vector<std::string>& texts, const std::string& text) {
    return std::ranges::find(texts, text) != texts.end();
}

}  // namespace

TEST_CASE("net pins: counter CLK", "[netlist]") {
    const auto design = Load("counter");
    const NetPins pins = DescribeNet(*design, "CLK");
    CHECK(pins.net == "CLK");
    CHECK_FALSE(pins.rail);
    REQUIRE(pins.drivers.size() == 1);
    CHECK(pins.drivers[0] == NetPin{"X1", 7, "OUT", PinRole::Output, ""});
    CHECK(Texts(pins.loads) == std::vector<std::string>{"U1 pin 2 CLK", "U2 pin 2 CLK",
                                                        "U3 pin 2 CLK", "U4 pin 2 CLK",
                                                        "U5 pin 2 CLK"});
}

TEST_CASE("net pins: pull-up", "[netlist]") {
    const auto design = Load("counter");
    const NetPins pins = DescribeNet(*design, "RESET_N");
    REQUIRE(pins.drivers.size() == 1);
    CHECK(pins.drivers[0].role == PinRole::PullUp);
    CHECK(Texts(pins.drivers) == std::vector<std::string>{"R1 pin 2 \u00b7 4k7 pull-up"});
    CHECK(Texts(pins.loads) == std::vector<std::string>{"U1 pin 1 CLR_N", "U2 pin 1 CLR_N",
                                                        "U3 pin 1 CLR_N", "U4 pin 1 CLR_N",
                                                        "U5 pin 1 CLR_N"});
}

TEST_CASE("net pins: series resistor on both its Nets", "[netlist]") {
    auto result =
        LoadNetlistFile(std::filesystem::path(BW_ROOT_DIR) / "tests/data/series_bench.bwn");
    REQUIRE(result.has_value());
    const NetPins a = DescribeNet(**result, "A");
    CHECK(Texts(a.drivers) == std::vector<std::string>{"R1 pin 1 \u00b7 1k series", "U1 pin 2 1Y"});
    const NetPins b = DescribeNet(**result, "B");
    CHECK(Texts(b.drivers) ==
          std::vector<std::string>{"R1 pin 2 \u00b7 1k series", "R2 pin 2 \u00b7 47k pull-up"});
}

TEST_CASE("net pins: a capacitor pulses its pulled Net", "[netlist]") {
    const auto loaded =
        LoadNetlistFile(std::filesystem::path(BW_ROOT_DIR) / "tests/data/capacitor_bench.bwn");
    REQUIRE(loaded.has_value());
    const NetPins a = DescribeNet(**loaded, "A");
    CHECK(Texts(a.drivers) == std::vector<std::string>{"U1 pin 2 1Y"});
    CHECK(Texts(a.loads) == std::vector<std::string>{"C1 pin 1 \u00b7 47p capacitor"});
    const NetPins b = DescribeNet(**loaded, "B");
    CHECK(Texts(b.drivers) == std::vector<std::string>{"C1 pin 2 \u00b7 47p capacitor",
                                                       "R1 pin 1 \u00b7 470 pull-down"});
}

TEST_CASE("net pins: rail", "[netlist]") {
    const auto design = Load("counter");
    const NetPins pins = DescribeNet(*design, "VCC");
    CHECK(pins.rail);
    CHECK(Texts(pins.drivers) == std::vector<std::string>{"J1 pin 1 VCC \u00b7 supply"});
    const auto r1 = std::ranges::find(pins.loads, "R1", &NetPin::reference);
    REQUIRE(r1 != pins.loads.end());
    CHECK(r1->pin == 0);
    CHECK(r1->role == PinRole::Load);
    CHECK(Contains(Texts(pins.loads), "U1 pin 16 VCC"));
}

TEST_CASE("net pins: video GND is a Load", "[netlist]") {
    const auto design = Load("test_pattern");
    const NetPins pins = DescribeNet(*design, "GND");
    CHECK(Texts(pins.drivers) == std::vector<std::string>{"J1 pin 2 GND \u00b7 supply"});
    CHECK(Contains(Texts(pins.loads), "J2 pin 3 GND"));
}

TEST_CASE("net pins: test pattern LINE_END_N", "[netlist]") {
    const auto design = Load("test_pattern");
    const NetPins pins = DescribeNet(*design, "LINE_END_N");
    CHECK(Texts(pins.drivers) == std::vector<std::string>{"U6 pin 6 1Y"});
    CHECK(Texts(pins.loads) == std::vector<std::string>{"U2 pin 9 LOAD_N", "U9 pin 5 3A"});
}

TEST_CASE("net pins: natural order", "[netlist]") {
    const auto design = Load("test_pattern");
    const NetPins pins = DescribeNet(*design, "CLK");
    CHECK(Texts(pins.drivers) == std::vector<std::string>{"X1 pin 8 OUT"});
    CHECK(Texts(pins.loads) ==
          std::vector<std::string>{"U1 pin 2 CLK", "U2 pin 2 CLK", "U3 pin 2 CLK", "U4 pin 2 CLK",
                                   "U5 pin 2 CLK", "U14 pin 3 1CLK", "U14 pin 11 2CLK"});
    CHECK(ReferenceLess("U2", "U10"));
    CHECK_FALSE(ReferenceLess("U10", "U2"));
    CHECK(ReferenceLess("R1", "U1"));
    CHECK(ReferenceLess("U", "U1"));
    CHECK_FALSE(ReferenceLess("U1", "U1"));
}

TEST_CASE("net pins: every pin once", "[netlist]") {
    const auto design = Load("test_pattern");
    std::size_t count = 0;
    std::set<std::pair<std::string, PinIndex>> seen;
    for (const auto& [name, id] : design->nets) {
        const NetPins pins = DescribeNet(*design, name);
        for (const auto* list : {&pins.drivers, &pins.loads}) {
            for (const NetPin& pin : *list) {
                CHECK(seen.emplace(pin.reference, pin.pin).second);
                ++count;
            }
        }
    }
    CHECK(count == design->connections.size());
}

TEST_CASE("net pins: unknown net", "[netlist]") {
    const auto design = Load("counter");
    const NetPins pins = DescribeNet(*design, "NOPE");
    CHECK_FALSE(pins.rail);
    CHECK(pins.drivers.empty());
    CHECK(pins.loads.empty());
}

TEST_CASE("net pins: FormatNetPin", "[netlist]") {
    CHECK(FormatNetPin({"U6", 5, "1Y", PinRole::Output, ""}) == "U6 pin 6 1Y");
    CHECK(FormatNetPin({"R1", 1, "", PinRole::PullUp, "4k7"}) == "R1 pin 2 \u00b7 4k7 pull-up");
    CHECK(FormatNetPin({"R2", 0, "", PinRole::PullDown, "1k"}) == "R2 pin 1 \u00b7 1k pull-down");
    CHECK(FormatNetPin({"J1", 0, "VCC", PinRole::Supply, ""}) == "J1 pin 1 VCC \u00b7 supply");
    CHECK(FormatNetPin({"U2", 8, "LOAD_N", PinRole::Load, ""}) == "U2 pin 9 LOAD_N");
}

TEST_CASE("net pins: a bidirectional pin is a Driver", "[netlist]") {
    auto result = LoadNetlist("part U1 2114\npart U2 74LS244\nnet B0 U1.IO1 U2.1Y1\n", "t.bwn");
    REQUIRE(result.has_value());
    const NetPins pins = DescribeNet(**result, "B0");
    CHECK(Texts(pins.drivers) == std::vector<std::string>{"U1 pin 14 IO1", "U2 pin 18 1Y1"});
    CHECK(pins.loads.empty());
}
