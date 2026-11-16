#include "devices/tv_input.hpp"

#include <filesystem>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "instruments/logic_probe.hpp"
#include "netlist/builder.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

struct Run {
    std::unique_ptr<LoadedCircuit> loaded;
    std::unique_ptr<Simulator> sim;
    std::unique_ptr<TvInput> input;
};

Run Start() {
    auto result = LoadNetlistFile(test::NetlistPath("test_pattern"));
    REQUIRE(result.has_value());
    Run run{std::move(*result), nullptr, nullptr};
    const auto video = ConnectorNet(*run.loaded, "VIDEO", "VIDEO");
    const auto syncN = ConnectorNet(*run.loaded, "VIDEO", "SYNC_N");
    REQUIRE(video.has_value());
    REQUIRE(syncN.has_value());
    run.sim = std::make_unique<Simulator>(run.loaded->circuit);
    run.sim->powerOn(1);
    run.input = std::make_unique<TvInput>(*run.sim, *video, *syncN);
    return run;
}

std::size_t Recorded(const Simulator& sim) {
    std::size_t total = 0;
    for (const NetId net : sim.recorder().nets()) {
        total += sim.recorder().samples(net).size();
    }
    return total;
}

}  // namespace

TEST_CASE("tv input: trim keeps the recorder bounded, keep draws the same", "[devices]") {
    Run trimmed = Start();
    Run kept = Start();
    for (int ms = 0; ms < 45; ++ms) {
        trimmed.sim->runFor(1_ms);
        trimmed.input->update();
        trimmed.sim->trimRecordings();
        CHECK(Recorded(*trimmed.sim) == 2);
        kept.sim->runFor(1_ms);
        kept.input->update();
    }
    CHECK(Recorded(*kept.sim) > 1000);
    REQUIRE(trimmed.input->tv().completedFrames() == kept.input->tv().completedFrames());
    CHECK(trimmed.input->tv().completedFrames() >= 2);
    CHECK(trimmed.input->tv().completedFrame().cells == kept.input->tv().completedFrame().cells);
}

TEST_CASE("tv input: probe and TV share a net", "[devices]") {
    Run alone = Start();
    Run shared = Start();
    LogicProbe probe(*shared.sim, *ConnectorNet(*shared.loaded, "VIDEO", "VIDEO"));
    bool pulsed = false;
    for (int ms = 0; ms < 45; ++ms) {
        alone.sim->runFor(1_ms);
        alone.input->update();
        shared.sim->runFor(1_ms);
        shared.input->update();
        probe.update();
        shared.sim->trimRecordings();
        pulsed = pulsed || probe.lamps().pulse;
    }
    CHECK(pulsed);
    REQUIRE(alone.input->tv().completedFrames() == shared.input->tv().completedFrames());
    CHECK(alone.input->tv().completedFrame().cells == shared.input->tv().completedFrame().cells);
}
