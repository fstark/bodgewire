#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
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

Session TestPatternSession() {
    return MakeSession(
        LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn"));
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
        Session session = TestPatternSession();
        return Frame(session, 1);
    }();
    return frame;
}

}  // namespace

TEST_CASE("bodge wire: broken trace repaired by a wire", "[machines]") {
    Session session =
        MakeSession(LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / "broken_trace.bwp"));
    session.setSpeed(Speed::Max);
    session.powerOn(1, 10_ms);
    session.runner().waitIdle();
    session.powerOff();
    REQUIRE(session.repair(WireRepair{{"U1", "14"}, {"U13", "6"}}) == RepairResult::Done);
    CHECK(Frame(session, 1) == FaultFree());
}

TEST_CASE("bodge wire: cut and mend on the test pattern", "[machines]") {
    Session session = TestPatternSession();
    REQUIRE(session.repair(CutRepair{Layer::Front, kH0Crack}) == RepairResult::Done);
    CHECK(Frame(session, 1) != FaultFree());
    REQUIRE(session.repair(MendRepair{Layer::Front, kH0Crack}) == RepairResult::Done);
    CHECK(Frame(session, 1) == FaultFree());
}
