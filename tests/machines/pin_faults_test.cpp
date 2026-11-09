#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "session/session.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

std::filesystem::path Root() { return std::filesystem::path(BW_NETLIST_DIR).parent_path(); }

Puzzle TestPattern() {
    Puzzle puzzle;
    puzzle.file = "t.bwp";
    puzzle.machineLine = 1;
    puzzle.machine = "test_pattern";
    return puzzle;
}

Machine Load(const Puzzle& puzzle) {
    auto machine = LoadMachine(puzzle, Root());
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

Session MakeSession(Machine machine) {
    auto built = machine.build();
    REQUIRE(built.has_value());
    return Session(std::move(machine), std::move(*built));
}

// TV frame at 60 ms (no lock needed: same seed, same Circuit, same frame).
std::vector<float> Frame(Session& session) {
    session.setSpeed(Speed::Max);
    session.powerOn(1, 60_ms);
    session.runner().waitIdle();
    REQUIRE(session.tv() != nullptr);
    REQUIRE(session.tv()->pictures().update());
    std::vector<float> cells = session.tv()->pictures().front().frame.cells;
    session.powerOff();
    return cells;
}

const std::vector<float>& FaultFree() {
    static const std::vector<float> frame = [] {
        Session session = MakeSession(Load(TestPattern()));
        return Frame(session);
    }();
    return frame;
}

}  // namespace

TEST_CASE("pin faults: a stuck input keeps its Net, the chip reads the value", "[machines]") {
    Puzzle puzzle = TestPattern();
    puzzle.stuck = {{2, "U13", "2B", LogicValue::One}};  // H0 into the grid NOR
    Machine machine = Load(puzzle);
    {
        const auto built = machine.build();
        REQUIRE(built.has_value());
        CHECK(PinNet(**built, "U13", "2B") == (*built)->nets.at("H0"));
    }
    Session session = MakeSession(std::move(machine));
    CHECK(Frame(session) != FaultFree());
    REQUIRE(session.repair(SwapRepair{"U13"}) == RepairResult::Done);
    CHECK(Frame(session) == FaultFree());
}
