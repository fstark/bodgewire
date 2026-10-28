#include "session/session.hpp"

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "machine/crack.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Session PuzzleSession(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    Session session(std::move(*machine), std::move(*built));
    session.setSpeed(Speed::Max);
    return session;
}

void RunTo(Session& session, Time t) {
    session.runner().pauseAt(t);
    session.runner().resume();
    session.runner().waitIdle();
}

// Point `nm` along the H0 Trace from the broken_trace crack.
Point Along(const Machine& machine, double nm) {
    const auto index = FindCrackTrace(machine.pristineBoard(), Layer::Front, kH0Crack);
    REQUIRE(index.has_value());
    const Trace& h0 = machine.pristineBoard().traces[*index];
    const double dx = static_cast<double>(h0.end.x - h0.start.x);
    const double dy = static_cast<double>(h0.end.y - h0.start.y);
    const double length = std::hypot(dx, dy);
    return Point{kH0Crack.x + static_cast<std::int64_t>(dx / length * nm),
                 kH0Crack.y + static_cast<std::int64_t>(dy / length * nm)};
}

}  // namespace

TEST_CASE("session: repairs only with the power off", "[session]") {
    Session session = PuzzleSession("broken_trace");
    session.powerOn(1);
    CHECK(session.repair(SwapRepair{"U3"}) == RepairResult::Refused);
    CHECK(session.log().empty());
    session.powerOff();
    CHECK(session.repair(SwapRepair{"U3"}) == RepairResult::Done);
    CHECK(session.repair(MendRepair{Layer::Front, {0, 0}}) == RepairResult::Done);
    const std::vector<std::string> log(session.log().begin(), session.log().end());
    CHECK(log == std::vector<std::string>{"swap U3", "mend F.Cu 0.00 0.00"});
    CHECK(session.blobs().size() == 1);
    CHECK(session.repair(SwapRepair{"U99"}) == RepairResult::Unknown);
    CHECK(session.log().size() == 2);
}

TEST_CASE("session: drawn Board unchanged by a mend", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const std::vector<Trace> before = session.drawnBoard()->traces;
    const std::size_t machineTraces = session.machine()->board().traces.size();
    REQUIRE(session.repair(MendRepair{Layer::Front, kH0Crack}) == RepairResult::Done);
    const std::vector<Trace>& after = session.drawnBoard()->traces;
    REQUIRE(after.size() == before.size());
    for (std::size_t t = 0; t < before.size(); ++t) {
        CHECK(after[t].start == before[t].start);
        CHECK(after[t].end == before[t].end);
    }
    CHECK(session.machine()->board().traces.size() == machineTraces - 1);
}

TEST_CASE("session: probe across a mend", "[session]") {
    Session session = PuzzleSession("broken_trace");
    const Point left = Along(*session.machine(), -1'000'000);
    const Point right = Along(*session.machine(), 1'000'000);
    session.touch(ProbeTip{left, Layer::Front});
    const auto a = session.probedNet();
    session.touch(ProbeTip{right, Layer::Front});
    const auto b = session.probedNet();
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(*a != *b);

    session.powerOn(1);
    RunTo(session, 1_ms);
    session.powerOff();
    REQUIRE(session.repair(MendRepair{Layer::Front, kH0Crack}) == RepairResult::Done);
    session.powerOn(1);
    REQUIRE(session.probeTip().has_value());
    CHECK(session.probeTip()->at == right);
    const auto rightNet = session.probedNet();
    session.touch(ProbeTip{left, Layer::Front});
    REQUIRE(rightNet.has_value());
    CHECK(session.probedNet() == rightNet);
    session.powerOff();
}

TEST_CASE("session: netlist without a Board", "[session]") {
    auto loaded = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "counter.bwn");
    REQUIRE(loaded.has_value());
    Session session(std::nullopt, std::move(*loaded));
    session.setSpeed(Speed::Max);
    CHECK(session.machine() == nullptr);
    CHECK(session.drawnBoard() == nullptr);
    CHECK(session.repair(SwapRepair{"U1"}) == RepairResult::Unknown);
    session.touch(ProbeTip{{0, 0}, Layer::Front});
    CHECK_FALSE(session.probeTip().has_value());
    session.powerOn(1, 1_ms);
    session.runner().waitIdle();
    CHECK(session.runner().status().simTime >= 1_ms);
}
