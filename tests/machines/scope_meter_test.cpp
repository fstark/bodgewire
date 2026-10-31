#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "machine/machine.hpp"

using namespace bw;

namespace {

Machine LoadPuzzle(const char* name) {
    auto machine =
        LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

}  // namespace

TEST_CASE("continuity: crack", "[machines]") {
    Machine machine = LoadPuzzle("broken_trace");
    const auto qa = machine.padPosition("U1", "14");
    const auto far = machine.padPosition("U13", "6");
    REQUIRE(qa.has_value());
    REQUIRE(far.has_value());
    CHECK_FALSE(machine.connected(Layer::Front, *qa, Layer::Front, *far));
    CHECK(machine.connected(Layer::Front, *qa, Layer::Back, *qa));
    REQUIRE(machine.mend(Layer::Front, kH0Crack));
    CHECK(machine.connected(Layer::Front, *qa, Layer::Front, *far));
}

TEST_CASE("continuity: off copper", "[machines]") {
    const Machine machine = LoadPuzzle("broken_trace");
    const auto qa = machine.padPosition("U1", "14");
    REQUIRE(qa.has_value());
    CHECK_FALSE(machine.connected(Layer::Front, {0, 0}, Layer::Front, {0, 0}));
    CHECK_FALSE(machine.connected(Layer::Front, *qa, Layer::Front, {0, 0}));
}
