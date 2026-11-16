#include "bw80_rig.hpp"

#include <unistd.h>

#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "netlist/diagnostic.hpp"
#include "netlist/parser.hpp"
#include "netlist/rom_directory.hpp"
#include "parts/key_matrix.hpp"
#include "parts/rom_contents.hpp"
#include "parts/zx80_keys.hpp"

namespace bw {

using namespace literals;

std::string Bw80NetlistText() {
    std::ifstream in(test::NetlistPath("bw80"));
    REQUIRE(in);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::uint8_t> Bw80Glyphs() {
    const auto rom = ReadRomContents(test::RomPath("bw80", "zx80_ic2.bin"), 4096, 8);
    REQUIRE(rom.has_value());
    return {rom->begin() + 0xE00, rom->end()};
}

std::filesystem::path Bw80TestRomDirectory() {
    // Cases run in parallel share it: never removed, the link replaced (by an atomic rename) when
    // it points elsewhere, as it did after the ROM moved (P6a).
    const auto dir = std::filesystem::temp_directory_path() / "bw80_test_roms";
    const auto rom = test::RomPath("bw80", "bw80_test.rom");
    const auto link = dir / "IC2.rom";
    std::filesystem::create_directories(dir);
    std::error_code error;
    if (std::filesystem::read_symlink(link, error) != rom) {
        const auto fresh = dir / ("IC2.rom." + std::to_string(::getpid()));
        std::filesystem::remove(fresh, error);
        std::filesystem::create_symlink(rom, fresh);
        std::filesystem::rename(fresh, link);
    }
    return dir;
}

std::unique_ptr<LoadedCircuit> Bw80Build(const std::string& text, bool testRom) {
    auto netlist = ParseNetlist(text, "bw80.bwn");
    REQUIRE(netlist.has_value());
    ResolveContentsPaths(*netlist, test::MachineDir("bw80"));
    if (testRom) {
        REQUIRE(ApplyRomDirectory(*netlist, Bw80TestRomDirectory()).has_value());
    }
    auto result = BuildCircuit(*netlist);
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

std::unique_ptr<LoadedCircuit> Bw80BoardCircuit(bool testRom) {
    auto machine = LoadNetlistMachine(
        test::NetlistPath("bw80"), testRom ? std::optional(Bw80TestRomDirectory()) : std::nullopt);
    if (!machine) {
        for (const Diagnostic& d : machine.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(machine.has_value());
    return Bw80Built(*machine);
}

std::unique_ptr<LoadedCircuit> Bw80Built(const Machine& machine) {
    auto built = machine.build();
    if (!built) {
        for (const Diagnostic& d : built.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(built.has_value());
    return std::move(*built);
}

namespace {

// The TV on J2: VIDEO and SYNC_N by connector pin.
std::unique_ptr<TvInput> PlugTv(Simulator& sim, const LoadedCircuit& loaded) {
    const auto video = ConnectorNet(loaded, "VIDEO", "VIDEO");
    const auto sync = ConnectorNet(loaded, "VIDEO", "SYNC_N");
    REQUIRE(video.has_value());
    REQUIRE(sync.has_value());
    return std::make_unique<TvInput>(sim, *video, *sync);
}

}  // namespace

Bw80Frames Bw80LockedFrames(LoadedCircuit& loaded, std::uint64_t seed, std::size_t count,
                            Time cap) {
    Simulator sim(loaded.circuit);
    sim.powerOn(seed);
    auto tv = PlugTv(sim, loaded);
    Bw80Frames result;
    std::uint64_t completed = 0;
    while (sim.now() < cap && result.frames.size() < count) {
        sim.runFor(1_ms);
        tv->update();
        sim.trimRecordings();
        if (tv->tv().completedFrames() != completed) {
            completed = tv->tv().completedFrames();
            if (tv->tv().isLocked()) {
                result.frames.push_back(tv->tv().completedFrame());
                result.stats.push_back(tv->tv().completedStats());
            }
        }
    }
    INFO("simulated " << sim.now() / 1'000'000'000 << " ms");
    return result;
}

void Bw80CheckSynced(const Bw80Frames& f) {
    for (std::size_t i = 1; i < f.frames.size(); ++i) {
        INFO("frame " << i << ": " << f.stats[i].lines << " lines, " << f.stats[i].syncedLines
                      << " synced");
        CHECK(f.stats[i].verticalSynced);
        CHECK(f.stats[i].lines >= 300);
        CHECK(f.stats[i].lines <= 320);
        CHECK(f.stats[i].syncedLines * 10 >= f.stats[i].lines * 9);
    }
}

PageDots Bw80TestRomPage() {
    const auto glyphs = Bw80Glyphs();
    return ExpectedPage(glyphs, kBw80TvColumns, kBw80TvRows, [](int row, int column) {
        const unsigned c = static_cast<unsigned>(row * kBw80TvColumns + column);
        return (c & 0x3F) | ((c & 0x40) << 1);
    });
}

Bw80Run::Bw80Run(LoadedCircuit& loaded, std::uint64_t seed) : sim(loaded.circuit) {
    sim.powerOn(seed);
    tv = PlugTv(sim, loaded);
}

void Bw80Run::runTo(Time t) {
    while (sim.now() < t) {
        sim.runFor(1_ms);
        tv->update();
        sim.trimRecordings();
    }
}

Bw80Frames Bw80Run::lockedFrames(std::size_t count, Time cap) {
    Bw80Frames result;
    completed = tv->tv().completedFrames();
    while (sim.now() < cap && result.frames.size() < count) {
        runTo(sim.now() + 1_ms);
        if (tv->tv().completedFrames() != completed) {
            completed = tv->tv().completedFrames();
            if (tv->tv().isLocked()) {
                result.frames.push_back(tv->tv().completedFrame());
                result.stats.push_back(tv->tv().completedStats());
            }
        }
    }
    return result;
}

void Bw80Type(Bw80Run& run, LoadedCircuit& loaded, std::u32string_view text, Time at, Time settle) {
    run.runTo(at);
    KeyMatrix* keyboard = KeyMatrixOf(loaded, "J3");
    REQUIRE(keyboard != nullptr);
    const std::size_t presses = Zx80Presses(text).size();
    REQUIRE(keyboard->type(text));
    run.sim.wake(keyboard->component().id());
    run.runTo(run.sim.now() + static_cast<Time>(presses) * 180_ms + settle);
}

std::vector<std::uint8_t> Bw80GlyphsMapped(std::span<const std::uint8_t> glyphs,
                                           const std::function<unsigned(unsigned)>& code) {
    std::vector<std::uint8_t> mapped(glyphs.size());
    for (unsigned c = 0; c < glyphs.size() / 8; ++c) {
        const unsigned from = code(c) & 0x3F;
        for (unsigned row = 0; row < 8; ++row) {
            mapped[c * 8 + row] = glyphs[from * 8 + row];
        }
    }
    return mapped;
}

std::vector<std::uint8_t> Bw80GlyphsRows(std::span<const std::uint8_t> glyphs,
                                         const std::array<int, 8>& rows) {
    std::vector<std::uint8_t> mapped(glyphs.size());
    for (std::size_t c = 0; c < glyphs.size() / 8; ++c) {
        for (std::size_t row = 0; row < 8; ++row) {
            mapped[c * 8 + row] = glyphs[c * 8 + static_cast<std::size_t>(rows[row])];
        }
    }
    return mapped;
}

}  // namespace bw
