#include <algorithm>
#include <cstdint>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "instruments/oscilloscope.hpp"
#include "machine/machine.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

Machine LoadPuzzle(const char* name) {
    auto machine =
        LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

}  // namespace

TEST_CASE("continuity: crack", "[machines]") {
    Machine machine = LoadPuzzle("broken_trace");
    const auto qa = machine.padPosition("U1", "14");
    const auto far = machine.padPosition("U13", "6");
    REQUIRE(qa.has_value());
    REQUIRE(far.has_value());
    CHECK_FALSE(machine.connected(Layer::Front, *qa, Layer::Front, *far));
    CHECK(machine.connected(Layer::Front, *qa, Layer::Back, *qa));
    REQUIRE(machine.mend(Layer::Front, kH0Crack));
    CHECK(machine.connected(Layer::Front, *qa, Layer::Front, *far));
}

TEST_CASE("continuity: off copper", "[machines]") {
    const Machine machine = LoadPuzzle("broken_trace");
    const auto qa = machine.padPosition("U1", "14");
    REQUIRE(qa.has_value());
    CHECK_FALSE(machine.connected(Layer::Front, {0, 0}, Layer::Front, {0, 0}));
    CHECK_FALSE(machine.connected(Layer::Front, *qa, Layer::Front, {0, 0}));
}

TEST_CASE("scope: LS161 clock to QA", "[machines]") {
    const Machine machine = LoadPuzzle("broken_trace");
    auto built = machine.build();
    REQUIRE(built.has_value());
    const auto clk = PinNet(**built, "U1", "2");
    const auto qa = PinNet(**built, "U1", "14");
    REQUIRE(clk.has_value());
    REQUIRE(qa.has_value());
    Simulator sim((*built)->circuit);
    sim.powerOn(1);
    Oscilloscope scope(sim, {clk, qa}, {.timebase = 20_ns});
    bool rise = false, fall = false;
    std::uint64_t seen = 0;
    // Odd slice length in clock periods, so the last Sweep alternates QA rising and falling.
    while (sim.now() < 20_ms) {
        sim.runUntil(sim.now() + 1_ms + 1_us);
        scope.update();
        const Sweep& sweep = scope.state().sweep();
        if (sweep.serial == seen) {
            continue;
        }
        seen = sweep.serial;
        CHECK(sweep.kind == SweepKind::Triggered);
        const Time trigger = sweep.start + sweep.timebase;
        const auto& samples = sweep.channels[1];
        const auto after =
            std::ranges::find_if(samples, [&](const Sample& s) { return s.time > trigger; });
        if (after == samples.end()) {
            continue;
        }
        const Time delay = after->time - trigger;
        if (after->value == LogicValue::One) {
            CHECK(delay == 13_ns);
            rise = true;
        } else {
            CHECK(delay == 15_ns);
            fall = true;
        }
    }
    CHECK(rise);
    CHECK(fall);
}
