#include <cmath>
#include <filesystem>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "machine/machine.hpp"
#include "runner/probe_consumer.hpp"
#include "runner/runner.hpp"
#include "view/board_scene.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

// Lamps at 300 ms with the probe on the pad of `reference`.`pin`, through the Runner.
std::string ProbeAt(const std::string& puzzle, const std::string& reference,
                    const std::string& pin) {
    const auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (puzzle + ".bwp"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    const auto at = machine->padPosition(reference, pin);
    REQUIRE(at.has_value());
    const auto net = machine->probeNet(**built, Layer::Front, *at);
    REQUIRE(net.has_value());

    ProbeConsumer probe;
    Runner runner((*built)->circuit, {&probe});
    runner.setSpeed(Speed::Max);
    probe.touch(runner, net);
    runner.pauseAt(300_ms);
    runner.powerOn(1);
    runner.waitIdle();
    return FormatLamps(probe.lamps());
}

}  // namespace

TEST_CASE("board probe: dead counter output", "[machines]") {
    CHECK(ProbeAt("dead_counter", "U3", "QA") == "LOW");
}

TEST_CASE("board probe: stuck gate output", "[machines]") {
    CHECK(ProbeAt("stuck_gate", "U12", "4Y") == "HIGH");
}

TEST_CASE("board probe: broken trace", "[machines]") {
    CHECK(ProbeAt("broken_trace", "U13", "2B") == "-");
    CHECK(ProbeAt("broken_trace", "U1", "QA").ends_with("PULSE"));
}

TEST_CASE("board probe: a pick's contact beside a Trace reads that Trace", "[machines]") {
    const auto machine =
        LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    const BoardScene scene(machine->board(), machine->pristineBoard(), machine->designCircuit());
    const Board& board = scene.board();
    constexpr double kPixelsPerMm = 20;
    const double eightPx = 8 / kPixelsPerMm * 1e6;
    const PickReach reach{static_cast<std::int64_t>(12 / kPixelsPerMm * 1e6),
                          static_cast<std::int64_t>(10 / kPixelsPerMm * 1e6)};
    std::size_t checked = 0;
    for (std::size_t t = 0; t < board.traces.size() && checked < 5; ++t) {
        const Trace& trace = board.traces[t];
        const double dx = static_cast<double>(trace.end.x - trace.start.x);
        const double dy = static_cast<double>(trace.end.y - trace.start.y);
        const double length = std::hypot(dx, dy);
        if (trace.mid || length < 3'000'000) {
            continue;
        }
        const double offset = static_cast<double>(trace.width) / 2 + eightPx;
        const Point at{(trace.start.x + trace.end.x) / 2 + std::llround(-dy / length * offset),
                       (trace.start.y + trace.end.y) / 2 + std::llround(dx / length * offset)};
        const auto picked = scene.pick(at, trace.layer, reach);
        if (!picked || picked->hit != Hit{HitKind::Trace, 0, t}) {
            continue;  // something nearer: a pad, via or other Trace
        }
        INFO("trace " << t);
        const auto expected = (*built)->nets.find(std::string(scene.netName(*scene.traceNet(t))));
        REQUIRE(expected != (*built)->nets.end());
        CHECK(machine->probeNet(**built, picked->layer, picked->contact) == expected->second);
        CHECK_FALSE(machine->probeNet(**built, picked->layer, at).has_value());
        ++checked;
    }
    CHECK(checked == 5);
}
