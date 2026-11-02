#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "dice.bwn");
    REQUIRE(result.has_value());
    return std::move(*result);
}

// After a roll: the count N0..N2 as a number ('x' bits give -1) and D1..D7 as "1" lit / "0" dark.
struct Face {
    int count;
    std::string leds;
};

Face Read(const LoadedCircuit& loaded, const Simulator& sim) {
    Face face{0, ""};
    for (int bit = 0; bit < 3; ++bit) {
        const LogicValue v = sim.value(*PinNet(loaded, "U1", std::string("Q") + char('A' + bit)));
        if (v != Zero && v != One) {
            face.count = -1;
        } else if (face.count >= 0 && v == One) {
            face.count |= 1 << bit;
        }
    }
    for (char d = '1'; d <= '7'; ++d) {
        const NetId k = *PinNet(loaded, std::string("D") + d, "K");
        face.leds += sim.value(k) == Zero && !sim.isFloating(k) ? '1' : '0';
    }
    return face;
}

// Seed 1, run 1 ms, hold SW1 for `held`, run 100 µs more.
Face Roll(LoadedCircuit& loaded, Time held) {
    Button* button = ButtonOf(loaded, "SW1");
    REQUIRE(button != nullptr);
    button->setPressed(false);
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    sim.runUntil(1_ms);
    button->setPressed(true);
    sim.wake(button->id());
    sim.runUntil(1_ms + held);
    button->setPressed(false);
    sim.wake(button->id());
    sim.runUntil(1_ms + held + 100_us);
    return Read(loaded, sim);
}

// LEDs D1..D7 of face n: TWO_N (D1, D7), FOUR_N (D2, D6), SIX_N (D3, D5), ODD_N (D4).
std::string Pattern(int n) {
    const char two = n >= 2 ? '1' : '0';
    const char four = n >= 4 ? '1' : '0';
    const char six = n == 6 ? '1' : '0';
    const char odd = n % 2 == 1 ? '1' : '0';
    return {two, four, six, odd, six, four, two};
}

}  // namespace

TEST_CASE("dice: netlist loads without warnings", "[machines]") {
    CHECK(DesignCircuit()->warnings.empty());
}

TEST_CASE("dice: six roll lengths give six faces", "[machines]") {
    const auto loaded = DesignCircuit();
    std::set<int> faces;
    for (int k = 0; k < 6; ++k) {
        const Face face = Roll(*loaded, 100_us + k * 1_us);
        INFO("k " << k << " count " << face.count << " leds " << face.leds);
        REQUIRE(face.count >= 1);
        REQUIRE(face.count <= 6);
        CHECK(face.leds == Pattern(face.count));
        faces.insert(face.count);
    }
    CHECK(faces == std::set<int>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("dice: the Board build rolls six faces", "[machines]") {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "dice.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    std::set<int> faces;
    for (int k = 0; k < 6; ++k) {
        faces.insert(Roll(**built, 100_us + k * 1_us).count);
    }
    CHECK(faces == std::set<int>{1, 2, 3, 4, 5, 6});
}

namespace {

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

// Rolls of 100 + k µs, k = 0 .. count - 1, each from a fresh power-on of one build.
std::vector<Face> Rolls(const Machine& machine, int count) {
    auto built = machine.build();
    REQUIRE(built.has_value());
    std::vector<Face> faces;
    for (int k = 0; k < count; ++k) {
        faces.push_back(Roll(**built, 100_us + k * 1_us));
    }
    return faces;
}

void CheckSixFaces(const Machine& machine) {
    std::set<int> counts;
    for (const Face& face : Rolls(machine, 6)) {
        INFO("count " << face.count << " leds " << face.leds);
        CHECK(face.leds == Pattern(face.count));
        counts.insert(face.count);
    }
    CHECK(counts == std::set<int>{1, 2, 3, 4, 5, 6});
}

}  // namespace

TEST_CASE("dice: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : {"dead_button", "loaded_dice", "seven", "odd_dice"}) {
        INFO(name);
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "dice");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("dice: dead_button", "[machines]") {
    Machine machine = LoadPuzzle("dead_button");
    {
        auto built = machine.build();
        REQUIRE(built.has_value());
        LoadedCircuit& loaded = **built;
        const NetId atSwitch = *PinNet(loaded, "SW1", "1");
        const NetId atGate = *PinNet(loaded, "U4", "3");
        REQUIRE(atSwitch != atGate);
        Button* button = ButtonOf(loaded, "SW1");
        REQUIRE(button != nullptr);
        Simulator sim(loaded.circuit);
        sim.powerOn(1);
        sim.runUntil(1_ms);
        const Face before = Read(loaded, sim);
        button->setPressed(true);
        sim.wake(button->id());
        sim.runUntil(1_ms + 100_us);
        CHECK(sim.value(atSwitch) == Zero);
        CHECK(sim.value(atGate) == One);
        button->setPressed(false);
        sim.wake(button->id());
        sim.runUntil(1_ms + 200_us);
        CHECK(sim.isFloating(atSwitch));
        CHECK(Read(loaded, sim).count == before.count);
    }
    REQUIRE(machine.mend(Layer::Front, kBtnNCrack));
    CheckSixFaces(machine);
}

TEST_CASE("dice: loaded_dice", "[machines]") {
    Machine machine = LoadPuzzle("loaded_dice");
    std::set<std::string> shown;
    for (const Face& face : Rolls(machine, 6)) {
        INFO("count " << face.count << " leds " << face.leds);
        CHECK((face.count == 1 || face.count == 2));
        shown.insert(face.leds);
    }
    CHECK(shown == std::set<std::string>{"0101010", "1110111"});  // a 3 and a 6

    REQUIRE(machine.mend(Layer::Back, kN2Crack));
    CheckSixFaces(machine);
}

TEST_CASE("dice: seven", "[machines]") {
    Machine machine = LoadPuzzle("seven");
    std::set<int> counts;
    for (const Face& face : Rolls(machine, 8)) {
        INFO("count " << face.count << " leds " << face.leds);
        CHECK(face.leds != Pattern(6));
        CHECK(face.leds[2] == '0');  // SIX_N never low
        counts.insert(face.count);
    }
    CHECK(counts.contains(0));
    CHECK(counts.contains(7));

    REQUIRE(machine.swap("U3"));
    CheckSixFaces(machine);
}

TEST_CASE("dice: odd_dice", "[machines]") {
    Machine machine = LoadPuzzle("odd_dice");
    for (const Face& face : Rolls(machine, 6)) {
        INFO("count " << face.count << " leds " << face.leds);
        CHECK(face.count % 2 == 1);
        CHECK(face.leds == (face.count == 7 ? std::string("1111111") : Pattern(face.count)));
    }

    REQUIRE(machine.swap("U1"));
    CheckSixFaces(machine);
}
