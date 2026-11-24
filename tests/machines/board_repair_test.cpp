#include <chrono>
#include <filesystem>
#include <print>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "../paths.hpp"
#include "session/session.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Session MakeSession(std::expected<Machine, std::vector<Diagnostic>> machine) {
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return Session(std::move(*machine), std::move(*built));
}

Session PuzzleSession(const std::string& name) {
    return MakeSession(LoadPuzzleFile(test::PuzzlePath(name)));
}

// TV frame at 60 ms (no lock needed: same seed, same Circuit, same frame).
std::vector<float> Frame(Session& session, std::uint64_t seed) {
    session.setSpeed(Speed::Max);
    session.powerOn(seed, 60_ms);
    session.runner().waitIdle();
    REQUIRE(session.tv() != nullptr);
    REQUIRE(session.tv()->pictures().update());
    std::vector<float> cells = session.tv()->pictures().front().frame.cells;
    session.powerOff();
    return cells;
}

const std::vector<float>& FaultFree() {
    static const std::vector<float> frame = [] {
        Session session = MakeSession(LoadNetlistMachine(test::NetlistPath("test_pattern")));
        return Frame(session, 1);
    }();
    return frame;
}

// The Puzzle repaired by `repair` after a first powered run, so the rebuild path is tested;
// `completed`: the verdict `repair` must leave (P8c).
std::vector<float> Repaired(const std::string& puzzle, const RepairAction& repair, bool completed) {
    Session session = PuzzleSession(puzzle);
    CHECK(session.puzzle());
    CHECK_FALSE(session.completed());
    session.setSpeed(Speed::Max);
    session.powerOn(1, 10_ms);
    session.runner().waitIdle();
    session.powerOff();
    REQUIRE(session.repair(repair) == RepairResult::Done);
    CHECK(session.completed() == completed);
    return Frame(session, 1);
}

}  // namespace

TEST_CASE("board repair: dead counter", "[machines]") {
    CHECK(Repaired("dead_counter", SwapRepair{"U3"}, true) == FaultFree());
    CHECK(Repaired("dead_counter", SwapRepair{"U4"}, false) != FaultFree());
}

TEST_CASE("board repair: stuck gate", "[machines]") {
    CHECK(Repaired("stuck_gate", SwapRepair{"U12"}, true) == FaultFree());
    CHECK(Repaired("stuck_gate", SwapRepair{"U13"}, false) != FaultFree());
}

TEST_CASE("board repair: broken trace", "[machines]") {
    CHECK(Repaired("broken_trace", MendRepair{H0Crack().layer, H0Crack().at}, true) == FaultFree());
    const Point away{H0Crack().at.x + 5'000'000, H0Crack().at.y};
    CHECK(Repaired("broken_trace", MendRepair{Layer::Front, away}, false) != FaultFree());
}

TEST_CASE("board repair: rebuild time", "[.perf]") {
    Session session = PuzzleSession("broken_trace");
    session.setSpeed(Speed::Max);
    REQUIRE(session.repair(WireRepair{{"U1", "14"}, {"U13", "6"}}) == RepairResult::Done);
    constexpr int kRebuilds = 20;
    std::chrono::duration<double, std::milli> total{0};
    for (int i = 0; i < kRebuilds; ++i) {
        session.powerOff();
        REQUIRE(session.repair(MendRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
        REQUIRE(session.repair(CutRepair{H0Crack().layer, H0Crack().at}) == RepairResult::Done);
        const auto start = std::chrono::steady_clock::now();
        session.powerOn(1, 1_ms);
        total += std::chrono::steady_clock::now() - start;
        session.runner().waitIdle();
    }
    const double mean = total.count() / kRebuilds;
    std::println("rebuild mean {:.1f} ms", mean);
    CHECK(mean < 100);
}
