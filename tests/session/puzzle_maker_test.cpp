#include "session/puzzle_maker.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "session/procedural_puzzle.hpp"

using namespace bw;

namespace {

std::filesystem::path Script(std::string_view machine) {
    return test::MachineDir(machine) / "puzzles/random.bwr";
}

// What the maker made, waited for (a minute at most: the Counter takes well under a second in
// release, a few in debug).
std::optional<std::expected<FaultDecl, std::string>> Wait(PuzzleMaker& maker) {
    for (int i = 0; i < 6000; ++i) {
        if (auto result = maker.take()) {
            return result;
        }
        if (!maker.making()) {
            return maker.take();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return std::nullopt;
}

}  // namespace

TEST_CASE("puzzle maker: makes one", "[session]") {
    PuzzleMaker maker;
    CHECK_FALSE(maker.making());
    CHECK_FALSE(maker.take().has_value());  // nothing started
    maker.start(Script("counter"), 3);
    const auto made = Wait(maker);
    REQUIRE(made.has_value());
    REQUIRE(made->has_value());
    CHECK_FALSE(maker.making());
    CHECK_FALSE(maker.take().has_value());  // taken once
    // The Fault the terminal makes for the same seed.
    const auto same = MakeProceduralPuzzleFor(Script("counter"), 3);
    REQUIRE(same.has_value());
    REQUIRE(same->has_value());
    CHECK(FormatFault(**made) == FormatFault((*same)->fault));
}

TEST_CASE("puzzle maker: stopped", "[session]") {
    PuzzleMaker maker;
    maker.start(Script("counter"), 1);
    maker.stop();
    CHECK_FALSE(maker.making());
    CHECK_FALSE(maker.take().has_value());  // forgotten: nothing to take, now or later
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK_FALSE(maker.take().has_value());
    maker.stop();  // nothing under way: no harm
}

TEST_CASE("puzzle maker: started again while making", "[session]") {
    PuzzleMaker maker;
    maker.start(Script("counter"), 1);
    maker.start(Script("counter"), 3);  // the first is stopped and forgotten
    const auto made = Wait(maker);
    REQUIRE(made.has_value());
    REQUIRE(made->has_value());
    const auto third = MakeProceduralPuzzleFor(Script("counter"), 3);
    REQUIRE(third.has_value());
    REQUIRE(third->has_value());
    CHECK(FormatFault(**made) == FormatFault((*third)->fault));
}

TEST_CASE("puzzle maker: a bad file is its error", "[session]") {
    PuzzleMaker maker;
    maker.start(test::RootDir() / "machines/none/random.bwr", 1);
    const auto made = Wait(maker);
    REQUIRE(made.has_value());
    REQUIRE_FALSE(made->has_value());
    CHECK(made->error() == "random.bwr: error: cannot read file");
}

TEST_CASE("puzzle maker: destroyed while making", "[session]") {
    {
        PuzzleMaker maker;
        maker.start(Script("counter"), 1);
    }  // stops and joins: no thread left behind
    SUCCEED();
}
