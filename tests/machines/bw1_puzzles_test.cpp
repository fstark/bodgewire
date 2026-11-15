// Machine 120 BW-1 Puzzles (docs/machines/120-bw1/BW1_DESIGN.md, Puzzles): each from power-on,
// faulty, read as the player reads it (the digits, the self-test's code), then repaired.

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "bw1_rig.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

// The Puzzles, in BW1.md's order.
constexpr const char* kPuzzles[] = {"odd_seven", "missing_bar", "half_segment", "bright_mess",
                                    "dead_da",   "dead_row",    "deaf_keypad"};

// Segment masks (bit s: segment 'A' + s), as the 002's TABLE without PA7.
constexpr std::uint8_t kZero = 0x3F;
constexpr std::uint8_t kTwo = 0x5B;
constexpr std::uint8_t kSeven = 0x07;
constexpr std::uint8_t kSegA = 0x01;
constexpr std::uint8_t kSegD = 0x08;
constexpr std::uint8_t kSegG = 0x40;

// The self-test's failure, strapped: test 1 fails within 150 ms of power-on.
std::string SelfTest(Machine& machine, Time until) {
    Bw1 bw1(Bw1Circuit(machine, true), 1);
    bw1.runUntil(until);
    return bw1.display();
}

// The Puzzle's Machine powered on with `seed`, the monitor started.
Bw1 Monitor(Machine& machine, std::uint64_t seed) { return Started(Bw1Circuit(machine), seed); }

}  // namespace

TEST_CASE("bw1: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : kPuzzles) {
        INFO(name);
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "bw1");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("bw1: odd_seven", "[machines][slow]") {
    Machine machine = LoadPuzzle("odd_seven");
    {
        Bw1 bw1 = Monitor(machine, 1);
        bw1.type("AD 0277");
        const auto lit = bw1.segments();
        CHECK(lit[0] == kZero);
        CHECK(lit[1] == kTwo);
        CHECK(lit[2] == (kSeven | kSegD));  // a 7 with a bottom bar
        CHECK(lit[3] == (kSeven | kSegD));
    }
    REQUIRE(machine.swap("U2"));
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("AD 0277");
    CheckShows(bw1.display(), "0277");
}

TEST_CASE("bw1: missing_bar", "[machines]") {
    Machine machine = LoadPuzzle("missing_bar");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        const auto lit = bw1.segments();
        CHECK(lit[0] == kZero);
        CHECK(lit[1] == (kTwo & ~kSegG));
        for (const std::uint8_t digit : lit) {
            CHECK((digit & kSegG) == 0);
        }
    }
    REQUIRE(machine.swap("U17"));
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: half_segment", "[machines]") {
    Machine machine = LoadPuzzle("half_segment");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        const auto lit = bw1.segments();
        CHECK(lit[0] == kZero);
        CHECK(lit[1] == kTwo);
        CHECK(lit[2] == kZero);
        CHECK(lit[3] == (kZero & ~kSegA));
        CHECK((lit[4] & kSegA) == 0);
        CHECK((lit[5] & kSegA) == 0);
    }
    REQUIRE(machine.mend(HalfSegmentCrack().layer, HalfSegmentCrack().at));
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: bright_mess", "[machines]") {
    Machine machine = LoadPuzzle("bright_mess");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        const auto lit = bw1.segments();
        CHECK(lit[0] == kZero);
        CHECK(lit[1] == kTwo);
        CHECK(lit[2] == 0x7F);  // lit with every digit: all their segments
        CHECK(lit[3] == kZero);
    }
    REQUIRE(machine.swap("U24"));
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: dead_da", "[machines][slow]") {
    Machine machine = LoadPuzzle("dead_da");
    {
        Bw1 bw1 = Monitor(machine, 1);
        bw1.type("456 DA 77");  // DA ignored: the 7s shift into the address
        CHECK(bw1.display().substr(0, 4) == "5677");
        bw1.type("AD 1A3B");  // A and 3 ignored
        CHECK(bw1.display().substr(0, 4) == "771B");
        CHECK((bw1.segments()[3] & kSegD) != 0);  // the B: U17 still drives SEG_D from PA3
    }
    REQUIRE(machine.mend(DeadDaCrack().layer, DeadDaCrack().at));
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("AD 0300 DA A3");
    CHECK(bw1.display() == "0300A3");
}

TEST_CASE("bw1: dead_row", "[machines][slow]") {
    Machine machine = LoadPuzzle("dead_row");
    {
        Bw1 bw1 = Monitor(machine, 1);
        bw1.type("456 EF");  // E and F ignored
        CHECK(bw1.display().substr(0, 4) == "0456");
        bw1.type("DA 77 AD 1 GO");  // DA, AD, GO ignored
        CHECK(bw1.display().substr(0, 4) == "6771");
    }
    REQUIRE(machine.mend(DeadRowCrack().layer, DeadRowCrack().at));
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("AD 0300 DA EF");
    CHECK(bw1.display() == "0300EF");
}

TEST_CASE("bw1: deaf_keypad", "[machines][slow]") {
    Machine machine = LoadPuzzle("deaf_keypad");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        const std::string before = bw1.display();
        CheckShows(before, "0200");
        bw1.type("AD 0300 DA A9");
        CHECK(bw1.display() == before);
    }
    {
        Bw1 bw1(Bw1Circuit(machine, true), 1);  // strapped: the self-test's keypad test
        bw1.runUntil(3200_ms);
        CHECK(bw1.display() == "PA5512");  // + held, no hand on the keypad
    }
    REQUIRE(machine.swap("SW19"));
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("AD 0300 DA A9");
    CHECK(bw1.display() == "0300A9");
}

TEST_CASE("bw1: bad_byte", "[machines][slow]") {
    Machine machine = LoadPuzzle("bad_byte");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        bw1.type("AD 0300 DA 00");
        CHECK(bw1.display() == "030080");
    }
    CHECK(SelfTest(machine, 150_ms) == "130080");
    REQUIRE(machine.swap("U5"));
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("AD 0300 DA 00");
    CHECK(bw1.display() == "030000");
}

TEST_CASE("bw1: dead_machine", "[machines]") {
    Machine machine = LoadPuzzle("dead_machine");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        bw1.runUntil(200_ms);
        CHECK(bw1.display() == "      ");
        bw1.press("RS");
        bw1.run(100_ms);
        CHECK(bw1.display() == "      ");
    }
    CHECK(SelfTest(machine, 150_ms) == "110008");
    REQUIRE(machine.swap("U9"));
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: wrong_suspect", "[machines][slow]") {
    Machine machine = LoadPuzzle("wrong_suspect");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        bw1.runUntil(200_ms);
        const std::string shown = bw1.display();
        CHECK(shown.substr(0, 4) != "0200");
        CHECK(shown.back() == ' ');
        bw1.type("AD 0300");
        CHECK(bw1.display() == shown);
    }
    CHECK(SelfTest(machine, 150_ms) == "100008");
    REQUIRE(machine.swap("U9"));  // the suspect is innocent
    CHECK(SelfTest(machine, 150_ms) == "100008");
    REQUIRE(machine.mend(WrongSuspectCrack().layer, WrongSuspectCrack().at));
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: half_byte", "[machines][slow]") {
    Machine machine = LoadPuzzle("half_byte");
    {
        Bw1 bw1 = Monitor(machine, 1);
        CHECK(*PinNet(bw1.loaded(), "U13", "1OE_N") != *PinNet(bw1.loaded(), "U15", "2Y"));
        CHECK(*PinNet(bw1.loaded(), "U13", "4OE_N") == *PinNet(bw1.loaded(), "U13", "1OE_N"));
        CHECK(*PinNet(bw1.loaded(), "U14", "1OE_N") == *PinNet(bw1.loaded(), "U15", "2Y"));
    }
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        CHECK(bw1.display() == "PA55  ");
        bw1.type("AD 0300");
        CHECK(bw1.display() == "PA55  ");
    }
    CHECK(SelfTest(machine, 150_ms) == "11000E");
    REQUIRE(machine.mend(HalfByteCrack().layer, HalfByteCrack().at));
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: aliased_page", "[machines][slow]") {
    Machine machine = LoadPuzzle("aliased_page");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        CHECK(bw1.display() == "1D2746");
        bw1.type("AD 0300");
        CHECK(bw1.display().substr(0, 4) == "0320");  // bit 5 of POINTL from the stack
    }
    CHECK(SelfTest(machine, 150_ms) == "100020");
    REQUIRE(machine.swap("U7"));
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}
