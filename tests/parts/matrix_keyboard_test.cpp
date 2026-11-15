#include "parts/matrix_keyboard.hpp"

#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "parts/catalogue.hpp"
#include "parts/connector.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

// The PET's keyboard side: a 74LS145 scanning row `row` (its BCD on ties), the columns pulled up
// by 10K, the BW-2001 keyboard on J5.
std::unique_ptr<LoadedCircuit> Bench(int row) {
    std::string text = "part J1 POWER\npart U1 74LS145\npart J5 MATRIXKB\n";
    for (int c = 0; c < 8; ++c) {
        text += "part R" + std::to_string(c + 1) + " R value=10k\n";
        text += "net COL" + std::to_string(c) + " J5.COL" + std::to_string(c) + " R" +
                std::to_string(c + 1) + ".2\nnet VCC R" + std::to_string(c + 1) + ".1\n";
    }
    for (int r = 0; r < 10; ++r) {
        text += "net ROW" + std::to_string(r) + " U1.Y" + std::to_string(r) + "_N J5.ROW" +
                std::to_string(r) + "\n";
    }
    const char* bits[] = {"A", "B", "C", "D"};
    for (int b = 0; b < 4; ++b) {
        text +=
            std::string("net ") + ((row >> b & 1) != 0 ? "VCC" : "GND") + " U1." + bits[b] + "\n";
    }
    auto loaded = LoadNetlist(text, "t.bwn");
    REQUIRE(loaded.has_value());
    CHECK((*loaded)->warnings.empty());
    return std::move(*loaded);
}

std::size_t Key(std::string_view legend) {
    const auto keys = Bw2001Keys();
    for (std::size_t k = 0; k < keys.size(); ++k) {
        if (keys[k].legend == legend) {
            return k;
        }
    }
    FAIL("no key " << legend);
    return 0;
}

}  // namespace

TEST_CASE("matrix keyboard: catalogue row, pins and keys", "[parts]") {
    const PartInfo* part = FindPart("MATRIXKB");
    REQUIRE(part != nullptr);
    CHECK(part->kind == PartKind::Connector);
    CHECK(part->package == "HDR-1x20");
    CHECK_FALSE(ConnectorPowersDevice("MATRIXKB"));
    MatrixKeyboard keyboard("J5");
    const auto pins = keyboard.pins();
    REQUIRE(pins.size() == 20);
    CHECK(pins[MatrixKeyboard::kRow0].name == "ROW0");
    CHECK(pins[9].name == "ROW9");
    CHECK(pins[MatrixKeyboard::kColumn0].name == "COL0");
    CHECK(pins[17].name == "COL7");
    CHECK(pins[18].kind == PinKind::NoConnect);
    CHECK(pins[0].sampled);
    CHECK(Bw2001Keys().size() == 73);
    CHECK(Bw2001KeyAt(6, 5) == Key("RETURN"));
    CHECK_FALSE(Bw2001KeyAt(1, 5).has_value());
}

TEST_CASE("matrix keyboard: a held key pulls its column low while its row is scanned", "[parts]") {
    auto loaded = Bench(2);
    MatrixKeyboard* keyboard = MatrixKeyboardOf(*loaded, "J5");
    REQUIRE(keyboard != nullptr);
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    sim.runUntil(1_us);
    CHECK(sim.value(loaded->nets.at("COL0")) == One);
    keyboard->setHeld(Key("Q"), true);  // row 2, column 0
    keyboard->setHeld(Key("W"), true);  // row 3, column 0: not scanned
    sim.wake(keyboard->id());
    sim.runUntil(2_us);
    CHECK(sim.value(loaded->nets.at("COL0")) == Zero);
    keyboard->setHeld(Key("Q"), false);
    sim.wake(keyboard->id());
    sim.runUntil(3_us);
    CHECK(sim.value(loaded->nets.at("COL0")) == One);
}

TEST_CASE("matrix keyboard: three keys in a rectangle ghost the fourth", "[parts]") {
    auto loaded = Bench(2);
    MatrixKeyboard* keyboard = MatrixKeyboardOf(*loaded, "J5");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    sim.runUntil(1_us);
    // E (2, 1), R (3, 1), W (3, 0): row 2's low reaches column 0 through row 3, as Q (2, 0) would.
    keyboard->setHeld(Key("E"), true);
    keyboard->setHeld(Key("R"), true);
    keyboard->setHeld(Key("W"), true);
    sim.wake(keyboard->id());
    sim.runUntil(2_us);
    CHECK(sim.value(loaded->nets.at("COL1")) == Zero);
    CHECK(sim.value(loaded->nets.at("COL0")) == Zero);
    CHECK(sim.value(loaded->nets.at("COL2")) == One);
}

TEST_CASE("matrix keyboard: a typed key is held 40 ms, then released 40 ms", "[parts]") {
    auto loaded = Bench(4);
    MatrixKeyboard* keyboard = MatrixKeyboardOf(*loaded, "J5");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    sim.runUntil(1_ms);
    REQUIRE(keyboard->type('a'));  // A: row 4, column 0
    REQUIRE(keyboard->type('D'));  // D: row 4, column 1
    sim.wake(keyboard->id());
    sim.runUntil(2_ms);
    CHECK(sim.value(loaded->nets.at("COL0")) == Zero);
    // A wake mid-press changes nothing.
    sim.wake(keyboard->id());
    sim.runUntil(40_ms);
    CHECK(sim.value(loaded->nets.at("COL0")) == Zero);
    sim.runUntil(42_ms);
    CHECK(sim.value(loaded->nets.at("COL0")) == One);
    CHECK(sim.value(loaded->nets.at("COL1")) == One);
    sim.runUntil(82_ms);
    CHECK(sim.value(loaded->nets.at("COL1")) == Zero);
    sim.runUntil(122_ms);
    CHECK(sim.value(loaded->nets.at("COL1")) == One);
}

TEST_CASE("matrix keyboard: typing with SHIFT, and the dropped queue", "[parts]") {
    auto loaded = Bench(8);  // row 8: SHIFT (left) on column 0
    MatrixKeyboard* keyboard = MatrixKeyboardOf(*loaded, "J5");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    REQUIRE(keyboard->type(kKeyClr));  // SHIFT + HOME
    REQUIRE(keyboard->type(kKeyClr));
    sim.wake(keyboard->id());
    sim.runUntil(10_ms);
    CHECK(sim.value(loaded->nets.at("COL0")) == Zero);
    keyboard->dropWaiting();
    sim.runUntil(100_ms);
    CHECK(sim.value(loaded->nets.at("COL0")) == One);  // the second was dropped
    CHECK_FALSE(keyboard->type(U'é'));
}

TEST_CASE("matrix keyboard: the character map", "[parts]") {
    const auto press = [](char32_t c) { return MatrixKeyFor(c); };
    REQUIRE(press('"'));
    CHECK(press('"')->key == Key("\""));
    CHECK_FALSE(press('"')->shift);
    CHECK(press('a')->key == Key("A"));
    CHECK(press('_')->key == Key("_"));
    CHECK(press('^')->key == Key("^"));
    CHECK(press('\n')->key == Key("RETURN"));
    CHECK(press(' ')->key == Key("SPACE"));
    CHECK(press(kKeyClr)->key == Key("HOME"));
    CHECK(press(kKeyClr)->shift);
    CHECK(press(kKeyInst)->key == Key("DEL"));
    CHECK(press(kKeyInst)->shift);
    CHECK(press(kKeyUp)->key == Key("CRSR DN"));
    CHECK(press(kKeyUp)->shift);
    CHECK_FALSE(press('{'));
    CHECK_FALSE(press('\t'));
    // Every printable character 0x20-0x5F has a key, as the editor types them.
    for (char32_t c = 0x20; c <= 0x5F; ++c) {
        INFO("character " << static_cast<int>(c));
        CHECK(press(c).has_value());
    }
}
