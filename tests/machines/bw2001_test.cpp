#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "bw2001_screen.hpp"
#include "devices/tv_input.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "parts/ram2114.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

std::filesystem::path NetlistPath() { return std::filesystem::path(BW_NETLIST_DIR) / "bw2001.bwn"; }

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(NetlistPath());
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Machine running, its TV on the MONITOR connector.
struct Running {
    std::unique_ptr<LoadedCircuit> loaded;
    std::unique_ptr<Simulator> sim;
    std::unique_ptr<TvInput> tv;
    const Ram2114* videoLow = nullptr;
    const Ram2114* videoHigh = nullptr;

    explicit Running(std::uint64_t seed, std::unique_ptr<LoadedCircuit> circuit = DesignCircuit())
        : loaded(std::move(circuit)) {
        sim = std::make_unique<Simulator>(loaded->circuit);
        sim->powerOn(seed);
        tv = std::make_unique<TvInput>(*sim,
                                       SeparateDrives{*ConnectorNet(*loaded, "MONITOR", "VIDEO_N"),
                                                      *ConnectorNet(*loaded, "MONITOR", "HDRIVE"),
                                                      *ConnectorNet(*loaded, "MONITOR", "VDRIVE")});
        videoLow = dynamic_cast<const Ram2114*>(loaded->components.at("UC3"));
        videoHigh = dynamic_cast<const Ram2114*>(loaded->components.at("UC4"));
    }
    void runUntil(Time t) {
        while (sim->now() < t) {
            sim->runUntil(std::min(t, sim->now() + 1_ms));
            tv->update();
            sim->trimRecordings();
        }
    }
    std::uint8_t screen(std::size_t offset) const {
        return static_cast<std::uint8_t>(videoLow->word(offset) | videoHigh->word(offset) << 4);
    }
};

}  // namespace

TEST_CASE("bw2001 machine: the netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    for (const Diagnostic& d : loaded->warnings) {
        UNSCOPED_INFO(Format(d));
    }
    CHECK(loaded->warnings.empty());
}

TEST_CASE("bw2001 machine: stage 1, the TV locks on 260 lines at 60 Hz", "[machines][slow]") {
    Running m(1);
    const NetId hdrive = *ConnectorNet(*m.loaded, "MONITOR", "HDRIVE");
    const NetId vdrive = *ConnectorNet(*m.loaded, "MONITOR", "VDRIVE");
    m.sim->record(hdrive);
    m.sim->record(vdrive);
    m.sim->runUntil(150_ms);  // the counters fall into step within a few frames of power-on
    m.tv->update();
    CHECK(m.tv->tv().isLocked());
    CHECK(m.tv->tv().completedStats().lines == 260);
    // HORZ DRIVE: high 24 us of every 64; VERT DRIVE: low 20 lines of every 260.
    const auto edges = [&](NetId net, LogicValue to) {
        std::vector<Time> at;
        for (const Sample& s : m.sim->recorder().samples(net)) {
            if (s.value == to && s.time > 100_ms) {
                at.push_back(s.time);
            }
        }
        return at;
    };
    const auto hRise = edges(hdrive, LogicValue::One), hFall = edges(hdrive, LogicValue::Zero);
    REQUIRE(hRise.size() > 10);
    CHECK(hRise[2] - hRise[1] == 64_us);  // one clock: both edges through the same gates
    const auto fall = std::ranges::upper_bound(hFall, hRise[1]);
    REQUIRE(fall != hFall.end());
    CHECK(*fall - hRise[1] > 24_us - 100'000);
    CHECK(*fall - hRise[1] < 24_us + 100'000);
    const auto vFall = edges(vdrive, LogicValue::Zero), vRise = edges(vdrive, LogicValue::One);
    REQUIRE(vFall.size() >= 2);
    CHECK(vFall[1] - vFall[0] == 260 * 64_us);
    const auto up = std::ranges::upper_bound(vRise, vFall[0]);
    REQUIRE(up != vRise.end());
    CHECK(*up - vFall[0] > 20 * 64_us - 100'000);  // within the gates' delays
    CHECK(*up - vFall[0] < 20 * 64_us + 100'000);
}

TEST_CASE("bw2001 machine: stage 1, the picture is the video RAM", "[machines][slow]") {
    const std::uint64_t seed = GENERATE(1u, 2u, 3u);
    INFO("seed " << seed);
    Running m(seed);
    m.runUntil(150_ms);
    REQUIRE(m.tv->tv().isLocked());
    const TvFrame& frame = m.tv->tv().completedFrame();
    CHECK(Bw2001BorderDots(frame) == 0);
    int wrong = 0;
    for (int cell = 0; cell < kBw2001Columns * kBw2001Rows; ++cell) {
        if (Bw2001CellDots(frame, cell) != Bw2001Glyph(m.screen(static_cast<std::size_t>(cell)))) {
            if (++wrong <= 3) {
                UNSCOPED_INFO("cell " << cell << " byte " << int(m.screen(cell)));
            }
        }
    }
    CHECK(wrong == 0);
}
