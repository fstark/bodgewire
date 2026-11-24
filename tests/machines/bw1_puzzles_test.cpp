// Machine 120 BW-1 Puzzles (machines/120-bw1/docs/BW1_DESIGN.md, Puzzles): each from power-on,
// faulty, read as the player reads it (the digits, the self-test's code), then repaired.

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "../paths.hpp"
#include "bw1_rig.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(test::PuzzlePath(name));
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

// Over `window`, whether the Net of `reference`.`pin` was ever low, and how often it changed.
struct Activity {
    bool low = false;
    int changes = 0;
};
Activity Watch(Bw1& bw1, std::string_view reference, std::string_view pin, Time window) {
    const auto net = PinNet(bw1.loaded(), reference, pin);
    REQUIRE(net.has_value());
    Activity seen;
    LogicValue last = bw1.sim().value(*net);
    const Time end = bw1.now() + window;
    while (bw1.now() < end) {
        bw1.run(1_us);
        const LogicValue value = bw1.sim().value(*net);
        seen.low = seen.low || value == LogicValue::Zero;
        seen.changes += value != last ? 1 : 0;
        last = value;
    }
    return seen;
}

// The Puzzle's Machine powered on with `seed`, the monitor started.
Bw1 Monitor(Machine& machine, std::uint64_t seed) { return Started(Bw1Circuit(machine), seed); }

}  // namespace

TEST_CASE("bw1: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : kPuzzles) {
        INFO(name);
        const auto puzzle = ReadPuzzleFile(test::PuzzlePath(name));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "bw1");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("bw1: odd_seven", "[machines][slow]") {
    Machine machine = LoadPuzzle("odd_seven");
    CHECK_FALSE(machine.completed());
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
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("AD 0277");
    CheckShows(bw1.display(), "0277");
}

TEST_CASE("bw1: missing_bar", "[machines]") {
    Machine machine = LoadPuzzle("missing_bar");
    CHECK_FALSE(machine.completed());
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
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: half_segment", "[machines]") {
    Machine machine = LoadPuzzle("half_segment");
    CHECK_FALSE(machine.completed());
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
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: bright_mess", "[machines]") {
    Machine machine = LoadPuzzle("bright_mess");
    CHECK_FALSE(machine.completed());
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
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: dead_da", "[machines][slow]") {
    Machine machine = LoadPuzzle("dead_da");
    CHECK_FALSE(machine.completed());
    {
        Bw1 bw1 = Monitor(machine, 1);
        bw1.type("456 DA 77");  // DA ignored: the 7s shift into the address
        CHECK(bw1.display().substr(0, 4) == "5677");
        bw1.type("AD 1A3B");  // A and 3 ignored
        CHECK(bw1.display().substr(0, 4) == "771B");
        CHECK((bw1.segments()[3] & kSegD) != 0);  // the B: U17 still drives SEG_D from PA3
    }
    REQUIRE(machine.mend(DeadDaCrack().layer, DeadDaCrack().at));
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("AD 0300 DA A3");
    CHECK(bw1.display() == "0300A3");
}

TEST_CASE("bw1: dead_row", "[machines][slow]") {
    Machine machine = LoadPuzzle("dead_row");
    CHECK_FALSE(machine.completed());
    {
        Bw1 bw1 = Monitor(machine, 1);
        bw1.type("456 EF");  // E and F ignored
        CHECK(bw1.display().substr(0, 4) == "0456");
        bw1.type("DA 77 AD 1 GO");  // DA, AD, GO ignored
        CHECK(bw1.display().substr(0, 4) == "6771");
    }
    REQUIRE(machine.mend(DeadRowCrack().layer, DeadRowCrack().at));
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("AD 0300 DA EF");
    CHECK(bw1.display() == "0300EF");
}

TEST_CASE("bw1: deaf_keypad", "[machines][slow]") {
    Machine machine = LoadPuzzle("deaf_keypad");
    CHECK_FALSE(machine.completed());
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
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("AD 0300 DA A9");
    CHECK(bw1.display() == "0300A9");
}

TEST_CASE("bw1: bad_byte", "[machines][slow]") {
    Machine machine = LoadPuzzle("bad_byte");
    CHECK_FALSE(machine.completed());
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        bw1.type("AD 0300 DA 00");
        CHECK(bw1.display() == "030080");
    }
    CHECK(SelfTest(machine, 150_ms) == "130080");
    REQUIRE(machine.swap("U5"));
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("AD 0300 DA 00");
    CHECK(bw1.display() == "030000");
}

TEST_CASE("bw1: dead_machine", "[machines]") {
    Machine machine = LoadPuzzle("dead_machine");
    CHECK_FALSE(machine.completed());
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
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: wrong_suspect", "[machines][slow]") {
    Machine machine = LoadPuzzle("wrong_suspect");
    CHECK_FALSE(machine.completed());
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
    CHECK_FALSE(machine.completed());
    CHECK(SelfTest(machine, 150_ms) == "100008");
    REQUIRE(machine.mend(WrongSuspectCrack().layer, WrongSuspectCrack().at));
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: half_byte", "[machines][slow]") {
    Machine machine = LoadPuzzle("half_byte");
    CHECK_FALSE(machine.completed());
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
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: aliased_page", "[machines][slow]") {
    Machine machine = LoadPuzzle("aliased_page");
    CHECK_FALSE(machine.completed());
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        CHECK(bw1.display() == "1D2746");
        bw1.type("AD 0300");
        CHECK(bw1.display().substr(0, 4) == "0320");  // bit 5 of POINTL from the stack
    }
    CHECK(SelfTest(machine, 150_ms) == "100020");
    REQUIRE(machine.swap("U7"));
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: self_test_fails", "[machines][slow]") {
    Machine machine = LoadPuzzle("self_test_fails");
    CHECK_FALSE(machine.completed());
    {
        Bw1 bw1 = Monitor(machine, 1);
        bw1.type("AD 0300 DA A9");
        CHECK(bw1.display() == "0300A9");
    }
    CHECK(SelfTest(machine, 150_ms) == "379504");  // test 3, $1795, bit 2
    REQUIRE(machine.swap("U3"));
    CHECK(machine.completed());
    CHECK(SelfTest(machine, 300_ms) == "888888");  // tests 1-5 passed
}

TEST_CASE("bw1: no_stop", "[machines][slow]") {
    Machine machine = LoadPuzzle("no_stop");
    CHECK_FALSE(machine.completed());
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        bw1.type("DA 00 AD 0200 GO");  // BRK
        CheckShows(bw1.display(), "0202");
        bw1.type("AD 0200 DA 4C + 00 + 02 AD 0200 GO");  // JMP $0200
        bw1.run(50_ms);
        bw1.press("ST");
        const std::string stopped = bw1.display();
        CHECK(stopped.substr(0, 3) != "020");  // not stopped in the loop
        bw1.type("AD 0300");
        CheckShows(bw1.display(), "0300");  // the monitor still answers
    }
    CHECK(SelfTest(machine, 150_ms) == "27FA01");
    REQUIRE(machine.swap("U2"));
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("DA 4C + 00 + 02 AD 0200 GO");
    bw1.run(50_ms);
    bw1.press("ST");
    CHECK(bw1.display() == "02004C");
}

TEST_CASE("bw1: rom_test_fails", "[machines][slow]") {
    Machine machine = LoadPuzzle("rom_test_fails");
    CHECK_FALSE(machine.completed());
    {
        Bw1 bw1 = Monitor(machine, 1);
        bw1.type("AD 0300 DA A9");
        CHECK(bw1.display() == "0300A9");
    }
    CHECK(SelfTest(machine, 150_ms) == "5800FF");  // test 5: U3's ROM sums to FF
    REQUIRE(machine.swap("U3"));
    CHECK(machine.completed());
    CHECK(SelfTest(machine, 300_ms) == "888888");
}

TEST_CASE("bw1: wrong_block", "[machines][slow]") {
    Machine machine = LoadPuzzle("wrong_block");
    CHECK_FALSE(machine.completed());
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        CHECK(bw1.display() == "      ");
        CHECK_FALSE(Watch(bw1, "U2", "RS0", 5_ms).low);    // K7: the monitor's ROM never selected
        CHECK_FALSE(Watch(bw1, "U2", "CS1", 5_ms).low);    // K5: nor its ports
        const Activity a10 = Watch(bw1, "U4", "A", 5_ms);  // the 6502 is stuck on $xFFF
        CHECK_FALSE(a10.low);                              // the leg reads A10 high...
        CHECK(Watch(bw1, "U3", "RS0", 5_ms).low);          // ...but U4 selects K6
    }
    CHECK(SelfTest(machine, 150_ms) == "      ");
    REQUIRE(machine.swap("U4"));
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: held_reset", "[machines]") {
    Machine machine = LoadPuzzle("held_reset");
    CHECK_FALSE(machine.completed());
    {
        Bw1 bw1 = Monitor(machine, 1);
        CHECK(bw1.display() == "      ");
        const Activity reset = Watch(bw1, "U1", "RES_N", 5_ms);
        CHECK(reset.low);
        CHECK(reset.changes == 0);
    }
    CHECK(SelfTest(machine, 150_ms) == "      ");
    REQUIRE(machine.swap("SW23"));
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: sst_kills", "[machines][slow]") {
    Machine machine = LoadPuzzle("sst_kills");
    CHECK_FALSE(machine.completed());
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        bw1.type("AD 0300");
        CheckShows(bw1.display(), "0300");
        bw1.setSst(true);
        bw1.run(20_ms);
        const auto lit = [](const std::string& shown) {
            return std::ranges::count_if(shown, [](char c) { return c != ' '; });
        };
        const std::string on = bw1.display();
        CHECK(lit(on) == 1);  // every digit dark but one
        bw1.type("AD 0400");
        CHECK(bw1.display() == on);
        bw1.setSst(false);
        bw1.run(20_ms);
        CHECK(bw1.display() == "1C0085");  // stopped in the monitor's SAVE
    }
    REQUIRE(machine.swap("U15"));
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    bw1.type("DA A9 + 11 + A9 + 22 AD 0200");  // LDA #$11, LDA #$22
    bw1.setSst(true);
    bw1.type("GO");
    CHECK(bw1.display() == "0202A9");
}

TEST_CASE("bw1: two_bits", "[machines][slow]") {
    Machine machine = LoadPuzzle("two_bits");
    CHECK_FALSE(machine.completed());
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Bw1 bw1 = Monitor(machine, seed);
        CHECK(*PinNet(bw1.loaded(), "U1", "D2") == *PinNet(bw1.loaded(), "U1", "D3"));
        const std::string shown = bw1.display();
        CHECK(shown.substr(0, 4) != "0200");
        bw1.press("RS");
        bw1.run(50_ms);
        CHECK(bw1.display().substr(0, 4) != "0200");
    }
    CHECK(SelfTest(machine, 150_ms).substr(0, 4) != "1   ");
    REQUIRE(machine.removeBridge({"U1", "31"}, {"U1", "30"}));
    CHECK(machine.completed());
    Bw1 bw1 = Monitor(machine, 1);
    CheckShows(bw1.display(), "0200");
}
