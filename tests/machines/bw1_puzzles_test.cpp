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

[[maybe_unused]] Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

// The Puzzles, in BW1.md's order.
constexpr const char* kPuzzles[] = {""};

}  // namespace

TEST_CASE("bw1: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : kPuzzles) {
        if (std::string(name).empty()) {
            continue;
        }
        INFO(name);
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "bw1");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}
