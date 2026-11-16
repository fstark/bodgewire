#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "session/session.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Puzzle TestPattern() {
    Puzzle puzzle;
    puzzle.file = "t.bwp";
    puzzle.machineLine = 1;
    puzzle.machine = "test_pattern";
    return puzzle;
}

Machine Load(const Puzzle& puzzle) {
    auto machine = LoadMachine(puzzle, test::MachineDir(puzzle.machine));
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

TEST_CASE("pin faults: a dry Joint's leg (F.Cu) and pad (B.Cu)", "[machines]") {
    Puzzle puzzle = TestPattern();
    puzzle.joints = {{2, {"U13", "2B"}}};  // H0 from U1.QA
    const Machine machine = Load(puzzle);
    const auto built = machine.build();
    REQUIRE(built.has_value());
    LoadedCircuit& loaded = **built;
    const NetId h0 = loaded.nets.at("H0");
    const Point leg = *machine.padPosition("U13", "2B");
    const Point qa = *machine.padPosition("U1", "QA");
    CHECK(machine.probeNet(loaded, Layer::Back, leg) == h0);
    CHECK(machine.probeNet(loaded, Layer::Front, leg) == PinNet(loaded, "U13", "2B"));
    CHECK(PinNet(loaded, "U13", "2B") != h0);
    CHECK(machine.probeNet(loaded, Layer::Front, qa) == h0);  // a good pin: leg and pad agree
    CHECK(machine.probeNet(loaded, Layer::Back, qa) == h0);

    CHECK_FALSE(machine.connected(Layer::Front, leg, Layer::Front, qa));
    CHECK_FALSE(machine.connected(Layer::Back, qa, Layer::Front, leg));
    CHECK_FALSE(machine.connected(Layer::Front, leg, Layer::Back, leg));  // leg to its own pad
    CHECK(machine.connected(Layer::Front, leg, Layer::Front, leg));
    CHECK(machine.connected(Layer::Back, leg, Layer::Front, qa));

    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    sim.runUntil(10_us);
    CHECK(sim.isFloating(*PinNet(loaded, "U13", "2B")));
}

TEST_CASE("pin faults: a dry Joint is fixed by resoldering, not by a swap", "[machines]") {
    Puzzle puzzle = TestPattern();
    puzzle.joints = {{2, {"U13", "2B"}}};
    Machine machine = Load(puzzle);
    CHECK(machine.swap("U13"));
    Session session = MakeSession(std::move(machine));
    CHECK(Frame(session) != FaultFree());
    REQUIRE(session.repair(SwapRepair{"U13"}) == RepairResult::Done);
    CHECK(Frame(session) != FaultFree());
}

TEST_CASE("pin faults: resolder through the Session", "[machines]") {
    Puzzle puzzle = TestPattern();
    puzzle.joints = {{2, {"U13", "2B"}}};
    Session session = MakeSession(Load(puzzle));
    const ResolderRepair resolder{{"U13", "2B"}};
    session.powerOn(1);
    CHECK(session.repair(resolder) == RepairResult::Refused);
    session.powerOff();
    CHECK(session.repair(ResolderRepair{{"J1", "1"}}) == RepairResult::Unknown);
    REQUIRE(session.repair(resolder) == RepairResult::Done);
    REQUIRE(session.log().size() == 1);
    CHECK(session.log()[0] == "resolder U13.2B");
    CHECK(Frame(session) == FaultFree());
}
