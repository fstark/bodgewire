#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "devices/tv_input.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "parts/rom_contents.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

// One dot is 4 TV columns (0.5 µs / 125 ns), one line one row.
constexpr int kDot = 4;
constexpr int kColumns = 16, kRows = 16;
constexpr int kCellDots = 6, kCellLines = 16;
constexpr int kPageDots = kColumns * kCellDots;  // 96
constexpr int kPageLines = kRows * kCellLines;   // 256

bool Lit(const TvFrame& frame, int row, int column) { return frame.at(row, column) >= 0.5f; }

// The page as dots, from the two ROM files: [line][dot].
std::vector<std::vector<bool>> ExpectedPage() {
    const std::filesystem::path roms = std::filesystem::path(BW_ROOT_DIR) / "roms";
    const auto message = ReadRomContents(roms / "message.rom", 2048, 8);
    const auto font = ReadRomContents(roms / "2513.rom", 512, 5);
    REQUIRE(message.has_value());
    REQUIRE(font.has_value());
    std::vector<std::vector<bool>> page(kPageLines, std::vector<bool>(kPageDots, false));
    for (int row = 0; row < kRows; ++row) {
        for (int column = 0; column < kColumns; ++column) {
            const unsigned code =
                (*message)[static_cast<std::size_t>(row * kColumns + column)] & 0x3F;
            for (int line = 0; line < kCellLines; ++line) {
                const unsigned dots = (*font)[code * 8 + static_cast<unsigned>(line / 2)];
                for (int x = 0; x < 5; ++x) {
                    page[static_cast<std::size_t>(row * kCellLines + line)]
                        [static_cast<std::size_t>(column * kCellDots + x)] = (dots >> (4 - x)) & 1;
                }
            }
        }
    }
    return page;
}

struct Offset {
    int left, top;
};

bool Matches(const TvFrame& frame, const std::vector<std::vector<bool>>& page, int left, int top) {
    for (int y = 0; y < kPageLines; ++y) {
        for (int x = 0; x < kPageDots; ++x) {
            const int column = left + kDot * x + kDot / 2;
            if (top + y >= kTvRows || column >= kTvColumns) {
                return false;
            }
            if (Lit(frame, top + y, column) !=
                page[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]) {
                return false;
            }
        }
    }
    return true;
}

// Where the page is on the frame: the middle of the one run of matching columns (a dot is 4
// columns wide); nullopt unless exactly one run of at most 4 matches.
std::optional<Offset> FindPage(const TvFrame& frame, const std::vector<std::vector<bool>>& page) {
    std::vector<Offset> matches;
    for (int top = 0; top < 64; ++top) {
        for (int left = 0; left < 128; ++left) {
            if (Matches(frame, page, left, top)) {
                matches.push_back({left, top});
            }
        }
    }
    if (matches.empty() || matches.size() > kDot) {
        return std::nullopt;
    }
    for (std::size_t i = 1; i < matches.size(); ++i) {
        if (matches[i].top != matches[0].top || matches[i].left != matches[i - 1].left + 1) {
            return std::nullopt;
        }
    }
    return matches[matches.size() / 2];
}

// Lit cells outside the text rectangle at `offset`.
int LitOutside(const TvFrame& frame, Offset offset) {
    int lit = 0;
    const int left = offset.left - kDot / 2;
    for (int row = 0; row < kTvRows; ++row) {
        for (int column = 0; column < kTvColumns; ++column) {
            const bool inside = row >= offset.top && row < offset.top + kPageLines &&
                                column >= left && column < left + kDot * kPageDots;
            lit += !inside && Lit(frame, row, column) ? 1 : 0;
        }
    }
    return lit;
}

// The Circuit built from the Board's Copper, with no Faults.
std::unique_ptr<LoadedCircuit> BoardCircuit() {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "message.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "message.bwn");
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

// Frames completed from power-on until `count` locked frames were seen (at most 1 s).
std::vector<TvFrame> LockedFrames(LoadedCircuit& loaded, std::uint64_t seed, int count) {
    Simulator sim(loaded.circuit);
    sim.powerOn(seed);
    TvInput tv(sim, *ConnectorNet(loaded, "VIDEO", "VIDEO"),
               *ConnectorNet(loaded, "VIDEO", "SYNC_N"));
    std::vector<TvFrame> frames;
    std::uint64_t completed = 0;
    while (sim.now() < 1_s && static_cast<int>(frames.size()) < count) {
        sim.runFor(1_ms);
        tv.update();
        sim.trimRecordings();
        if (tv.tv().completedFrames() != completed) {
            completed = tv.tv().completedFrames();
            if (tv.tv().isLocked()) {
                frames.push_back(tv.tv().completedFrame());
            }
        }
    }
    return frames;
}

}  // namespace

TEST_CASE("message: netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    for (const Diagnostic& d : loaded->warnings) {
        UNSCOPED_INFO(Format(d));
    }
    CHECK(loaded->warnings.empty());
}

TEST_CASE("message: the TV shows the page", "[machines][slow]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    const auto page = ExpectedPage();
    auto loaded = BoardCircuit();
    // The first locked frame may have started before the counters fell into step.
    const auto frames = LockedFrames(*loaded, seed, 3);
    REQUIRE(frames.size() == 3);
    const auto offset = FindPage(frames[1], page);
    REQUIRE(offset.has_value());
    INFO("left " << offset->left << ", top " << offset->top);
    CHECK(offset->left >= 41);
    CHECK(offset->left <= 47);
    CHECK(offset->top <= 8);
    CHECK(LitOutside(frames[1], *offset) == 0);
    CHECK(frames[2].cells == frames[1].cells);
}
