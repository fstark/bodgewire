#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

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

Session PuzzleSession(const char* name) {
    return MakeSession(LoadPuzzleFile(test::PuzzlePath(name)));
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
        Session session = MakeSession(LoadNetlistMachine(test::NetlistPath("test_pattern")));
        return Frame(session);
    }();
    return frame;
}

void CheckRepairs(const char* puzzle, const char* repair) {
    INFO(puzzle << " " << repair);
    Session session = PuzzleSession(puzzle);
    CHECK(Frame(session) != FaultFree());
    const auto parsed = ParseRepair(repair);
    REQUIRE(parsed.has_value());
    REQUIRE(session.repair(*parsed) == RepairResult::Done);
    CHECK(Frame(session) == FaultFree());
}

}  // namespace

TEST_CASE("shorts power: each Puzzle is fixed by its Repair", "[machines]") {
    CheckRepairs("sync_bridge.bwp", "unbridge:U10.1:U10.2");
    CheckRepairs("line_bridge.bwp", "unbridge:U9.6:U9.7");
    CheckRepairs("unpowered_chip.bwp", "mend:F.Cu:164.85:51.05");
    CheckRepairs("dead_rail.bwp", "unbridge:U1.7:U1.8");
}

TEST_CASE("shorts power: swapping the chip does not fix a bridge", "[machines]") {
    Session session = PuzzleSession("line_bridge.bwp");
    REQUIRE(session.repair(SwapRepair{"U9"}) == RepairResult::Done);
    CHECK(Frame(session) != FaultFree());
}

TEST_CASE("shorts power: a wire to VCC powers the chip", "[machines]") {
    Session session = PuzzleSession("unpowered_chip.bwp");
    REQUIRE(session.repair(WireRepair{{"U6", "14"}, {"U14", "14"}}) == RepairResult::Done);
    CHECK(Frame(session) == FaultFree());
}

TEST_CASE("shorts power: Dead Rail limits current and reads 0 V", "[machines]") {
    Session session = PuzzleSession("dead_rail.bwp");
    const auto vcc = session.machine()->padPosition("U1", "16");
    REQUIRE(vcc.has_value());
    REQUIRE(session.placeLead({*vcc, Layer::Back, "U1.16 VCC"}));
    CHECK_FALSE(session.currentLimited());

    session.setSpeed(Speed::Max);
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    CHECK(session.currentLimited());
    CHECK(session.volts() == 0.0);
    session.powerOff();
    CHECK_FALSE(session.currentLimited());

    REQUIRE(session.repair(UnbridgeRepair{{"U1", "7"}, {"U1", "8"}}) == RepairResult::Done);
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    CHECK_FALSE(session.currentLimited());
    CHECK(session.volts() == 5.0);
    session.powerOff();
}
