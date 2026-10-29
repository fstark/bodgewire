#include "instruments/logic_analyzer.hpp"

#include <array>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "machine/machine.hpp"
#include "netlist/builder.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

// Capture at 1 MHz, all Any, of the given pads in the built `puzzle`.
Capture CaptureOf(const std::string& puzzle,
                  std::initializer_list<std::pair<const char*, const char*>> pins) {
    const auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (puzzle + ".bwp"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    std::array<std::optional<NetId>, kChannels> channels;
    std::size_t c = 0;
    for (const auto& [reference, pin] : pins) {
        channels[c] = PinNet(**built, reference, pin);
        REQUIRE(channels[c].has_value());
        ++c;
    }
    Simulator sim((*built)->circuit);
    sim.powerOn(1);
    LogicAnalyzer analyzer(sim, channels);
    analyzer.arm({.period = 1_us});
    while (analyzer.state().phase() != AnalyzerPhase::Captured && sim.now() < 100_ms) {
        sim.runUntil(sim.now() + 1_ms);
        analyzer.update();
    }
    REQUIRE(analyzer.state().phase() == AnalyzerPhase::Captured);
    return analyzer.state().capture();
}

std::size_t Changes(const Capture& capture, std::size_t channel) {
    std::size_t changes = 0;
    for (std::size_t p = capture.first + 1; p < capture.points.size(); ++p) {
        const auto bit = [&](std::size_t i) {
            return (capture.points[i].high >> channel & 1) |
                   (capture.points[i].unknown >> channel & 1) << 1;
        };
        changes += bit(p) != bit(p - 1) ? 1 : 0;
    }
    return changes;
}

}  // namespace

TEST_CASE("analyzer: crack stops H0", "[machines]") {
    const Capture capture = CaptureOf("broken_trace", {{"U1", "14"}, {"U13", "6"}});
    CHECK(Changes(capture, 0) > 0);
    CHECK(Changes(capture, 1) == 0);
}

TEST_CASE("analyzer: stuck gate", "[machines]") {
    const Capture capture = CaptureOf("stuck_gate", {{"U12", "11"}});
    for (std::size_t p = capture.first; p < kDepth; ++p) {
        CHECK((capture.points[p].unknown & 1) == 0);
        CHECK((capture.points[p].high & 1) == 1);
    }
}
