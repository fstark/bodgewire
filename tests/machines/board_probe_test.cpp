#include <filesystem>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "machine/machine.hpp"
#include "runner/probe_consumer.hpp"
#include "runner/runner.hpp"

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
