#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "machine/machine.hpp"
#include "netlist/net_pins.hpp"
#include "view/board_scene.hpp"

using namespace bw;

namespace {

std::vector<std::string> Texts(const std::vector<NetPin>& pins) {
    std::vector<std::string> texts;
    for (const NetPin& pin : pins) {
        texts.push_back(FormatNetPin(pin));
    }
    return texts;
}

}  // namespace

TEST_CASE("net panel: crack does not split H0", "[machines]") {
    const auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / "broken_trace.bwp");
    REQUIRE(machine.has_value());
    const NetPins pins = DescribeNet(machine->designCircuit(), "H0");
    CHECK(Texts(pins.drivers) == std::vector<std::string>{"U1 pin 14 QA"});
    CHECK(Texts(pins.loads) == std::vector<std::string>{"U13 pin 6 2B"});

    const BoardScene scene(machine->board(), machine->pristineBoard(), machine->designCircuit());
    const auto u1 = scene.componentIndex("U1");
    const auto u13 = scene.componentIndex("U13");
    REQUIRE(u1.has_value());
    REQUIRE(u13.has_value());
    const auto qa = scene.padIndex(*u1, "14");
    const auto b2 = scene.padIndex(*u13, "6");
    REQUIRE(qa.has_value());
    REQUIRE(b2.has_value());
    const auto net = scene.padNet(*u1, *qa);
    REQUIRE(net.has_value());
    CHECK(scene.netName(*net) == "H0");
    CHECK(scene.padNet(*u13, *b2) == net);
}

TEST_CASE("net panel: stuck output is still an output", "[machines]") {
    const auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / "stuck_gate.bwp");
    REQUIRE(machine.has_value());
    const NetPins pins = DescribeNet(machine->designCircuit(), "GRID");
    REQUIRE(pins.drivers.size() == 1);
    CHECK(pins.drivers[0].role == PinRole::Output);
    CHECK(Texts(pins.drivers) == std::vector<std::string>{"U12 pin 11 4Y"});
    CHECK(Texts(pins.loads) == std::vector<std::string>{"U11 pin 10 3B"});
}
