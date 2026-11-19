#include "parts/membrane_keyboard.hpp"

#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "parts/catalogue.hpp"
#include "parts/connector.hpp"
#include "parts/zx80_keys.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

// The ZX80's keyboard side: the five columns pulled up by 47k, the rows on ties (`lowRows`: the
// bits of A8-A15 held low, the others high; a row in `floating` on a Net of its own), the BW-80
// keyboard on J3.
std::unique_ptr<LoadedCircuit> Bench(unsigned lowRows, unsigned floating = 0) {
    std::string text = "part J1 POWER\npart J3 MEMBRANEKB\n";
    for (int c = 0; c < 5; ++c) {
        text += "part R" + std::to_string(c + 13) + " R value=47k\n";
        text += "net KB" + std::to_string(c) + " J3.KB" + std::to_string(c) + " R" +
                std::to_string(c + 13) + ".1\nnet VCC R" + std::to_string(c + 13) + ".2\n";
    }
    for (int r = 0; r < 8; ++r) {
        const std::string pin = "J3.A" + std::to_string(8 + r);
        if ((floating >> r & 1) != 0) {
            text += "net A" + std::to_string(8 + r) + " " + pin + "\n";
        } else {
            text +=
                std::string("net ") + ((lowRows >> r & 1) != 0 ? "GND" : "VCC") + " " + pin + "\n";
        }
    }
    auto loaded = LoadNetlist(text, "t.bwn");
    REQUIRE(loaded.has_value());
    CHECK((*loaded)->warnings.empty());
    return std::move(*loaded);
}

std::size_t Key(std::string_view legend) {
    const auto keys = Zx80Keys();
    for (std::size_t k = 0; k < keys.size(); ++k) {
        if (keys[k].legend == legend) {
            return k;
        }
    }
    FAIL("no key " << legend);
    return 0;
}

}  // namespace

TEST_CASE("membrane keyboard: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("MEMBRANEKB");
    REQUIRE(part != nullptr);
    CHECK(part->kind == PartKind::Connector);
    CHECK(part->package == "HDR-1x13");
    CHECK_FALSE(ConnectorPowersDevice("MEMBRANEKB"));
    MembraneKeyboard keyboard("J3");
    CHECK(keyboard.kind() == KeyMatrixKind::Bw80);
    CHECK(keyboard.keyCount() == 40);
    CHECK(keyboard.shiftKey() == Key("SHIFT"));
    const auto pins = keyboard.pins();
    REQUIRE(pins.size() == 13);
    CHECK(pins[MembraneKeyboard::kRow0].name == "A8");
    CHECK(pins[7].name == "A15");
    CHECK(pins[MembraneKeyboard::kColumn0].name == "KB0");
    CHECK(pins[12].name == "KB4");
    CHECK(pins[0].kind == PinKind::Input);
    CHECK_FALSE(pins[0].sampled);
    CHECK(pins[8].kind == PinKind::OpenCollector);
}

TEST_CASE("membrane keyboard: a held key pulls its column low while its row is low", "[parts]") {
    auto loaded = Bench(1u << 2);  // A10 low: Q W E R T
    KeyMatrix* keyboard = KeyMatrixOf(*loaded, "J3");
    REQUIRE(keyboard != nullptr);
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    sim.runUntil(1_us);
    CHECK(sim.value(loaded->nets.at("KB0")) == One);
    keyboard->setHeld(Key("Q"), true);  // row 2, column 0
    keyboard->setHeld(Key("A"), true);  // row 1, column 0: its row is high
    sim.wake(keyboard->component().id());
    sim.runUntil(2_us);
    CHECK(sim.value(loaded->nets.at("KB0")) == Zero);
    CHECK(sim.value(loaded->nets.at("KB1")) == One);
    keyboard->setHeld(Key("Q"), false);
    sim.wake(keyboard->component().id());
    sim.runUntil(3_us);
    CHECK(sim.value(loaded->nets.at("KB0")) == One);
}

TEST_CASE("membrane keyboard: SHIFT with a letter never joins two rows", "[parts]") {
    auto loaded = Bench(1u << 0);  // A8 low, A9 high
    KeyMatrix* keyboard = KeyMatrixOf(*loaded, "J3");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    keyboard->setHeld(Key("SHIFT"), true);  // (0, 0)
    keyboard->setHeld(Key("A"), true);      // (1, 0): the same column
    sim.wake(keyboard->component().id());
    sim.runUntil(2_us);
    CHECK(sim.value(loaded->nets.at("KB0")) == Zero);
    CHECK(sim.value(loaded->nets.at("KB1")) == One);
    // Rows 0 and 1 are rails here: were they joined through the keys, the Net would fight.
    CHECK(sim.value(loaded->nets.at("GND")) == Zero);
    CHECK(sim.value(loaded->nets.at("VCC")) == One);
}

TEST_CASE("membrane keyboard: a key on a floating row does nothing", "[parts]") {
    auto loaded = Bench(0, 1u << 2);  // A10 on a Net of its own
    KeyMatrix* keyboard = KeyMatrixOf(*loaded, "J3");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    keyboard->setHeld(Key("Q"), true);
    sim.wake(keyboard->component().id());
    sim.runUntil(2_us);
    CHECK(sim.value(loaded->nets.at("KB0")) == One);
}

TEST_CASE("membrane keyboard: a typed key is held 60 ms, then released 120 ms", "[parts]") {
    auto loaded = Bench((1u << 1) | (1u << 0) | (1u << 5));  // A9, A8 and A13 low
    KeyMatrix* keyboard = KeyMatrixOf(*loaded, "J3");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    sim.runUntil(1_ms);
    REQUIRE(keyboard->type(U"a\""));  // A (1, 0); then SHIFT (0, 0) with Y (5, 4)
    sim.wake(keyboard->component().id());
    sim.runUntil(2_ms);
    CHECK(sim.value(loaded->nets.at("KB0")) == Zero);
    CHECK(keyboard->scripted(Key("A")));
    sim.runUntil(60_ms);
    CHECK(sim.value(loaded->nets.at("KB0")) == Zero);
    sim.runUntil(62_ms);
    CHECK(sim.value(loaded->nets.at("KB0")) == One);
    CHECK(sim.value(loaded->nets.at("KB4")) == One);
    sim.runUntil(180_ms);
    CHECK(sim.value(loaded->nets.at("KB0")) == One);
    sim.runUntil(182_ms);
    CHECK(sim.value(loaded->nets.at("KB0")) == Zero);  // SHIFT
    CHECK(sim.value(loaded->nets.at("KB4")) == Zero);  // Y
    CHECK(keyboard->scripted(Key("SHIFT")));
    CHECK(keyboard->scripted(Key("Y")));
    sim.runUntil(242_ms);
    CHECK(sim.value(loaded->nets.at("KB0")) == One);
    CHECK(sim.value(loaded->nets.at("KB4")) == One);
    CHECK_FALSE(keyboard->type(U"{}"));
}

TEST_CASE("membrane keyboard: a press queued as it is", "[parts]") {
    auto loaded = Bench((1u << 1) | (1u << 0));  // A9 and A8 low
    KeyMatrix* keyboard = KeyMatrixOf(*loaded, "J3");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    REQUIRE(keyboard->press({Key("A"), true}));  // SHIFT with A: the grey graphic
    sim.wake(keyboard->component().id());
    sim.runUntil(10_ms);
    CHECK(sim.value(loaded->nets.at("KB0")) == Zero);  // SHIFT (0, 0) and A (1, 0)
    CHECK(keyboard->scripted(Key("SHIFT")));
    CHECK(keyboard->scripted(Key("A")));
    sim.runUntil(70_ms);
    CHECK(sim.value(loaded->nets.at("KB0")) == One);
    CHECK_FALSE(keyboard->press({kZx80KeyCount, false}));
}

TEST_CASE("membrane keyboard: the dropped queue", "[parts]") {
    auto loaded = Bench(1u << 1);
    KeyMatrix* keyboard = KeyMatrixOf(*loaded, "J3");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    REQUIRE(keyboard->type(U"AS"));
    sim.wake(keyboard->component().id());
    sim.runUntil(10_ms);
    CHECK(sim.value(loaded->nets.at("KB0")) == Zero);
    keyboard->dropWaiting();
    sim.runUntil(300_ms);
    CHECK(sim.value(loaded->nets.at("KB1")) == One);  // S was dropped
}
