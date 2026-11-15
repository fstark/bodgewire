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
constexpr const char* kPuzzles[] = {"odd_seven", "missing_bar", "half_segment", "bright_mess"};

// Segment masks (bit s: segment 'A' + s), as the 002's TABLE without PA7.
constexpr std::uint8_t kZero = 0x3F;
constexpr std::uint8_t kTwo = 0x5B;
constexpr std::uint8_t kSeven = 0x07;
constexpr std::uint8_t kSegA = 0x01;
constexpr std::uint8_t kSegD = 0x08;
constexpr std::uint8_t kSegG = 0x40;

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
