#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../paths.hpp"
#include "devices/tv_input.hpp"
#include "netlist/builder.hpp"
#include "netlist/diagnostic.hpp"
#include "parts/ram2114.hpp"
#include "parts/rom_contents.hpp"
#include "parts/z80_stub.hpp"
#include "sim/simulator.hpp"
#include "tv_page.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

// The stub's page through the glyphs of the netlist's ROM (at $0600), D7 inverting.
PageDots StubPage(const char* romFile) {
    const auto rom = ReadRomContents(test::BenchPath(romFile), 2048, 8);
    REQUIRE(rom.has_value());
    const auto& page = Z80Stub::Page();
    return ExpectedPage(std::span(*rom).subspan(0x600, 64 * 8), Z80Stub::kColumns, Z80Stub::kRows,
                        [&](int row, int column) {
                            return unsigned{
                                page[static_cast<std::size_t>(row * Z80Stub::kRowBytes + column)]};
                        });
}

// The stub's netlist (P5a) and the real Z80's (P5s), with their ROMs.
struct BusLoad {
    const char* file;
    const char* rom;
    // Time to 3 locked frames. The stub's first VSYNC comes at once; the Z80's after the POR
    // and the page copy (6 ms), and the TV, free-running at 21 ms, accepts it only 16-21 ms into
    // its own frame: about 6 frames to fall in step.
    Time cap;
};
constexpr BusLoad kStub{"bus_load.bwn", "bus_load.rom", 100'000'000'000};
constexpr BusLoad kZ80{"bus_load_z80.bwn", "bus_load_z80.rom", 250'000'000'000};

std::unique_ptr<LoadedCircuit> Load(const BusLoad& netlist) {
    auto result = LoadNetlistFile(test::BenchPath(netlist.file));
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

}  // namespace

TEST_CASE("bus load: netlist loads without warnings", "[machines]") {
    const BusLoad netlist = GENERATE(kStub, kZ80);
    INFO(netlist.file);
    const auto loaded = Load(netlist);
    for (const Diagnostic& d : loaded->warnings) {
        UNSCOPED_INFO(Format(d));
    }
    CHECK(loaded->warnings.empty());
}

TEST_CASE("bus load: the TV shows the page", "[machines][slow]") {
    const BusLoad netlist = GENERATE(kStub, kZ80);
    const std::uint64_t seed = GENERATE(1, 2);
    INFO(netlist.file << ", seed " << seed);
    const auto loaded = Load(netlist);
    Simulator sim(loaded->circuit);
    sim.powerOn(seed);
    TvInput tv(sim, loaded->nets.at("VIDEO"), loaded->nets.at("SYNC_N"));
    std::vector<TvFrame> frames;
    std::vector<TvFrameStats> stats;
    std::uint64_t completed = 0;
    while (sim.now() < netlist.cap && frames.size() < 3) {
        sim.runFor(1_ms);
        tv.update();
        sim.trimRecordings();
        if (tv.tv().completedFrames() != completed) {
            completed = tv.tv().completedFrames();
            if (tv.tv().isLocked()) {
                frames.push_back(tv.tv().completedFrame());
                stats.push_back(tv.tv().completedStats());
            }
        }
    }
    REQUIRE(frames.size() == 3);
    for (std::size_t i = 1; i < 3; ++i) {
        CHECK(stats[i].verticalSynced);
        // During the 4 lines of vertical sync the TV runs 3 lines of its own (66 us); the first
        // HSYNCs after it come too early for its line oscillator, until it falls back in step.
        CHECK(stats[i].lines >= Z80Stub::kFrameLines - 2);
        CHECK(stats[i].lines <= Z80Stub::kFrameLines);
        CHECK(stats[i].syncedLines * 10 >= stats[i].lines * 9);
    }
    const auto page = StubPage(netlist.rom);
    const auto at = FindPage(frames[1], page);
    REQUIRE(at.has_value());
    INFO("left " << at->first << ", top " << at->second);
    CHECK(Matches(frames[2], page, at->first, at->second));
}

TEST_CASE("bus load: the Z80 program copies the page", "[machines]") {
    const auto loaded = Load(kZ80);
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    sim.runFor(10_ms);  // the POR's 1 ms, then the copy (about 5 ms)
    // RAM at $4000: 6 border HALTs, then the page; U4 holds the low nibble, U5 the high.
    const auto& low = static_cast<const Ram2114&>(*loaded->components.at("U4"));
    const auto& high = static_cast<const Ram2114&>(*loaded->components.at("U5"));
    const auto& page = Z80Stub::Page();
    for (std::size_t i = 0; i < page.size(); ++i) {
        INFO("page byte " << i);
        const unsigned value = static_cast<unsigned>(high.word(6 + i)) << 4 | low.word(6 + i);
        CHECK(value == page[i]);
    }
}
