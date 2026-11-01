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

TEST_CASE("bodge wire: unbridge through the Session", "[machines]") {
    Puzzle puzzle;
    puzzle.file = "t.bwp";
    puzzle.machineLine = 1;
    puzzle.machine = "test_pattern";
    puzzle.bridges = {{2, {"U1", "7"}, {"U1", "8"}}};
    Session session =
        MakeSession(LoadMachine(puzzle, std::filesystem::path(BW_NETLIST_DIR).parent_path()));
    const auto pad = [&](const char* pin) {
        const auto at = session.machine()->padPosition("U1", pin);
        REQUIRE(at.has_value());
        return TestLead{*at, Layer::Back, ""};
    };
    session.setMeterMode(MeterMode::Continuity);
    REQUIRE(session.placeLead(pad("7")));
    REQUIRE(session.placeLead(pad("8")));
    CHECK(session.continuity() == true);

    const UnbridgeRepair unbridge{{"U1", "7"}, {"U1", "8"}};
    session.setSpeed(Speed::Max);
    session.powerOn(1);
    CHECK(session.repair(unbridge) == RepairResult::Refused);
    session.powerOff();
    CHECK(session.repair(unbridge) == RepairResult::Done);
    REQUIRE(session.log().size() == 1);
    CHECK(session.log()[0] == "unbridge U1.7 U1.8");
    CHECK(session.continuity() == false);
    CHECK(session.repair(unbridge) == RepairResult::Unknown);
}
