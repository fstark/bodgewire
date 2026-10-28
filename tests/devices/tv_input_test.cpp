#include "devices/tv_input.hpp"

#include <filesystem>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

struct Run {
    std::unique_ptr<LoadedCircuit> loaded;
    std::unique_ptr<Simulator> sim;
    std::unique_ptr<TvInput> input;
};

Run Start(TvInput::History history) {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(result.has_value());
    Run run{std::move(*result), nullptr, nullptr};
    const auto video = ConnectorNet(*run.loaded, "VIDEO", "VIDEO");
    const auto syncN = ConnectorNet(*run.loaded, "VIDEO", "SYNC_N");
    REQUIRE(video.has_value());
    REQUIRE(syncN.has_value());
    run.sim = std::make_unique<Simulator>(run.loaded->circuit);
    run.sim->powerOn(1);
    run.input = std::make_unique<TvInput>(*run.sim, *video, *syncN, history);
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
    Run trimmed = Start(TvInput::History::Trim);
    Run kept = Start(TvInput::History::Keep);
    for (int ms = 0; ms < 45; ++ms) {
        trimmed.sim->runFor(1_ms);
        trimmed.input->update();
        CHECK(Recorded(*trimmed.sim) == 2);
        kept.sim->runFor(1_ms);
        kept.input->update();
    }
    CHECK(Recorded(*kept.sim) > 1000);
    REQUIRE(trimmed.input->tv().completedFrames() == kept.input->tv().completedFrames());
    CHECK(trimmed.input->tv().completedFrames() >= 2);
    CHECK(trimmed.input->tv().completedFrame().cells == kept.input->tv().completedFrame().cells);
}
