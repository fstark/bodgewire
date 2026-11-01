#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
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

Machine Load(std::string_view puzzle) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / puzzle);
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

ScenePad PadOf(const BoardScene& scene, std::string_view reference, std::string_view pin) {
    const auto component = scene.componentIndex(reference);
    REQUIRE(component.has_value());
    const auto pad = scene.padIndex(*component, pin);
    REQUIRE(pad.has_value());
    return {*component, *pad};
}

std::vector<ScenePad> Drivers(const BoardScene& scene, ScenePad on) {
    const auto net = scene.padNet(on.component, on.pad);
    REQUIRE(net.has_value());
    const auto drivers = scene.drivers(*net);
    return {drivers.begin(), drivers.end()};
}

}  // namespace

TEST_CASE("net drivers: crack does not split H0", "[machines]") {
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

TEST_CASE("net drivers: stuck output is still an output", "[machines]") {
    const auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / "stuck_gate.bwp");
    REQUIRE(machine.has_value());
    const NetPins pins = DescribeNet(machine->designCircuit(), "GRID");
    REQUIRE(pins.drivers.size() == 1);
    CHECK(pins.drivers[0].role == PinRole::Output);
    CHECK(Texts(pins.drivers) == std::vector<std::string>{"U12 pin 11 4Y"});
    CHECK(Texts(pins.loads) == std::vector<std::string>{"U11 pin 10 3B"});
}

TEST_CASE("net drivers: crack does not move the mark", "[machines]") {
    const Machine machine = Load("broken_trace.bwp");
    const BoardScene scene(machine.board(), machine.pristineBoard(), machine.designCircuit());
    const ScenePad qa = PadOf(scene, "U1", "14");
    CHECK(Drivers(scene, PadOf(scene, "U13", "6")) == std::vector<ScenePad>{qa});
}

TEST_CASE("net drivers: stuck output keeps its mark", "[machines]") {
    const Machine machine = Load("stuck_gate.bwp");
    const BoardScene scene(machine.board(), machine.pristineBoard(), machine.designCircuit());
    const ScenePad y4 = PadOf(scene, "U12", "11");
    CHECK(Drivers(scene, y4) == std::vector<ScenePad>{y4});
}

TEST_CASE("net drivers: pull-up and supply", "[machines]") {
    const Machine machine = Load("dark_led.bwp");
    const BoardScene scene(machine.board(), machine.pristineBoard(), machine.designCircuit());
    CHECK(Drivers(scene, PadOf(scene, "U1", "1")) == std::vector{PadOf(scene, "R1", "2")});
    CHECK(Drivers(scene, PadOf(scene, "U1", "8")) == std::vector{PadOf(scene, "J1", "2")});
}
