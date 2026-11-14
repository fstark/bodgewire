#include "parts/power_on_reset.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "parts/switch.hpp"
#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

std::expected<Component*, std::string> Create(Circuit& circuit, std::vector<PartParam> params) {
    return FindPart("POR")->create(circuit, "U1", params);
}

}  // namespace

TEST_CASE("POR: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("POR");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-8");
    CHECK(part->kind == PartKind::Chip);
    PowerOnReset chip("U1", 1_ms);
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 8);
    CHECK(pins[PowerOnReset::kRstN].name == "RST_N");
    CHECK(pins[PowerOnReset::kRstN].kind == PinKind::OpenCollector);
    CHECK(pins[3].name == "GND");
    CHECK(pins[7].name == "VCC");
    CHECK(pins[4].kind == PinKind::NoConnect);
}

TEST_CASE("POR: time parameter", "[parts]") {
    Circuit circuit;
    CHECK(Create(circuit, {{"time", "20ms"}}).has_value());
    CHECK(Create(circuit, {}).error() == "POR needs time=<duration>");
    CHECK(Create(circuit, {{"hold", "1ms"}}).error() == "POR: unknown parameter 'hold'");
    CHECK(Create(circuit, {{"time", "100ns"}}).error() == "POR: time must be at least 1us");
    CHECK_FALSE(Create(circuit, {{"time", "soon"}}).has_value());
}

TEST_CASE("POR: low from power-on for its time, then released to the pull-up", "[parts]") {
    // RST_N with a pull-up and a button to GND on the same Net, as on a board.
    Circuit circuit;
    const NetId gnd = circuit.addNet("GND");
    const NetId rst = circuit.addNet("RES_N");
    circuit.tie(gnd, Zero, DriveStrength::Supply);
    circuit.tie(rst, One, DriveStrength::Weak);
    auto& por = circuit.add<PowerOnReset>("U1", 20_us);
    circuit.connect(por, PowerOnReset::kRstN, rst);
    auto& button = *static_cast<Switch*>(*FindPart("BUTTON")->create(circuit, "SW1", {}));
    const std::vector<std::string> nets{"GND", "RES_N"};
    auto& network = circuit.add<SwitchNetwork>(nets);
    circuit.connect(network, 0, gnd);
    circuit.connect(network, 1, rst);
    network.add(button, 0, 1);
    circuit.finalize();
    for (const std::uint64_t seed : {1u, 2u, 3u}) {
        Simulator sim(circuit);
        sim.powerOn(seed);
        CHECK(sim.value(rst) == Zero);
        sim.runUntil(20_us - 1);
        CHECK(sim.value(rst) == Zero);
        sim.runUntil(20_us);
        CHECK(sim.value(rst) == One);
        sim.runUntil(1_ms);
        CHECK(sim.value(rst) == One);
        button.setPressed(true);  // the button still pulls the line low afterwards
        sim.wake(button.id());
        sim.runUntil(1_ms + 1_us);
        CHECK(sim.value(rst) == Zero);
        button.setPressed(false);
        sim.wake(button.id());
        sim.runUntil(1_ms + 2_us);
        CHECK(sim.value(rst) == One);
    }
}
