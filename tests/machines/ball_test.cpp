#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "devices/tv_input.hpp"
#include "instruments/logic_probe.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

// One dot is 4 TV columns (0.5 µs / 125 ns), one line one row.
constexpr int kDot = 4;
constexpr int kWallColumns = 4 * kDot;  // side walls: 4 dots
constexpr int kWallRows = 16;           // top and bottom walls: 16 lines
constexpr int kBall = 8;                // 2 dots wide, 8 lines tall

bool Lit(const TvFrame& frame, int row, int column) { return frame.at(row, column) >= 0.5f; }

struct Box {
    int left = 0, right = 0;  // inside edges: first and last column inside the side walls
    int top = 0, bottom = 0;  // inside edges: first and last row inside the top and bottom walls
    int leftWall = 0;         // first column of the left wall
    int rightWall = 0;        // last column of the right wall
    int topWallRows = 0, bottomWallRows = 0;
    int netColumn() const { return left + 44 * kDot; }  // dots 48–49: 44 dots after dot 4
};

struct Ball {
    int left, top, width, height;
};

struct Picture {
    std::optional<Box> box;
    std::vector<int> netRows;  // inside rows with the net lit
    std::optional<Ball> ball;  // lit cells inside the box, not wall or net
    int litOutside = 0;        // lit cells outside the box and its walls
};

// Reads the box from the frame itself: full-width rows are the top and bottom walls; the
// outermost runs of columns lit on every row between them are the side walls.
Picture Read(const TvFrame& frame) {
    Picture picture;
    std::vector<int> fullRows;
    for (int row = 0; row < kTvRows; ++row) {
        int lit = 0;
        for (int column = 0; column < kTvColumns; ++column) {
            lit += Lit(frame, row, column) ? 1 : 0;
        }
        if (lit >= 352) {
            fullRows.push_back(row);
        }
    }
    if (fullRows.size() < 2) {
        return picture;
    }
    // The first run and the last run of full rows.
    Box box;
    int topEnd = fullRows.front();
    while (std::ranges::find(fullRows, topEnd + 1) != fullRows.end()) {
        ++topEnd;
    }
    int bottomStart = fullRows.back();
    while (std::ranges::find(fullRows, bottomStart - 1) != fullRows.end()) {
        --bottomStart;
    }
    if (bottomStart <= topEnd + 1) {
        return picture;
    }
    box.top = topEnd + 1;
    box.bottom = bottomStart - 1;
    box.topWallRows = topEnd - fullRows.front() + 1;
    box.bottomWallRows = fullRows.back() - bottomStart + 1;
    std::vector<bool> full(kTvColumns, false);
    for (int column = 0; column < kTvColumns; ++column) {
        bool all = true;
        for (int row = box.top; row <= box.bottom && all; ++row) {
            all = Lit(frame, row, column);
        }
        full[column] = all;
    }
    const auto first = std::ranges::find(full, true);
    if (first == full.end()) {
        return picture;
    }
    box.leftWall = static_cast<int>(first - full.begin());
    box.left = box.leftWall;
    while (box.left < kTvColumns && full[box.left]) {
        ++box.left;
    }
    box.rightWall = kTvColumns - 1;
    while (box.rightWall >= 0 && !full[box.rightWall]) {
        --box.rightWall;
    }
    box.right = box.rightWall;
    while (box.right >= 0 && full[box.right]) {
        --box.right;
    }
    if (box.right <= box.left) {
        return picture;
    }
    picture.box = box;

    const int net = box.netColumn();
    for (int row = box.top; row <= box.bottom; ++row) {
        if (Lit(frame, row, net) && Lit(frame, row, net + 2 * kDot - 1)) {
            picture.netRows.push_back(row);
        }
    }
    int minRow = kTvRows, maxRow = -1, minColumn = kTvColumns, maxColumn = -1;
    for (int row = 0; row < kTvRows; ++row) {
        for (int column = 0; column < kTvColumns; ++column) {
            if (!Lit(frame, row, column)) {
                continue;
            }
            const bool inBox =
                row >= box.top && row <= box.bottom && column >= box.left && column <= box.right;
            // The top wall's first row is line 0, drawn 32 columns early (see `ball: box and
            // dashed net`): its stray cells are not counted.
            if (row == box.top - box.topWallRows) {
                continue;
            }
            const bool inWalls = row >= box.top - box.topWallRows &&
                                 row <= box.bottom + box.bottomWallRows && column >= box.leftWall &&
                                 column <= box.rightWall;
            if (!inWalls) {
                ++picture.litOutside;
            }
            if (!inBox || (column >= net && column < net + 2 * kDot)) {
                continue;
            }
            minRow = std::min(minRow, row);
            maxRow = std::max(maxRow, row);
            minColumn = std::min(minColumn, column);
            maxColumn = std::max(maxColumn, column);
        }
    }
    if (maxRow >= 0) {
        picture.ball = Ball{minColumn, minRow, maxColumn - minColumn + 1, maxRow - minRow + 1};
    }
    return picture;
}

// Runs the TV from power-on in 1 ms steps (Tv keeps only the last completed frame) and calls
// `onFrame` for each frame completed that started at or after `from`. Returns the probes' lamps.
std::vector<std::string> Watch(LoadedCircuit& loaded, std::uint64_t seed, Time from, Time to,
                               const std::function<void(const TvFrame&, bool locked)>& onFrame,
                               const std::vector<std::string>& probes = {}) {
    Simulator sim(loaded.circuit);
    sim.powerOn(seed);
    TvInput tv(sim, *ConnectorNet(loaded, "VIDEO", "VIDEO"),
               *ConnectorNet(loaded, "VIDEO", "SYNC_N"));
    std::vector<LogicProbe> lamps;
    for (const std::string& probe : probes) {
        const std::size_t dot = probe.find('.');
        const auto net = PinNet(loaded, probe.substr(0, dot), probe.substr(dot + 1));
        REQUIRE(net.has_value());
        lamps.emplace_back(sim, *net);
    }
    std::uint64_t frames = 0;
    while (sim.now() < to) {
        sim.runFor(1_ms);
        tv.update();
        for (LogicProbe& probe : lamps) {
            probe.update();
        }
        sim.trimRecordings();
        if (tv.tv().completedFrames() != frames) {
            frames = tv.tv().completedFrames();
            if (tv.tv().completedStats().start >= from) {
                onFrame(tv.tv().completedFrame(), tv.tv().isLocked());
            }
        }
    }
    std::vector<std::string> seen;
    for (const LogicProbe& probe : lamps) {
        seen.push_back(FormatLamps(probe.lamps()));
    }
    return seen;
}

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "ball.bwn");
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Circuit built from the Board's Copper, with no Faults.
std::unique_ptr<LoadedCircuit> BoardCircuit() {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "ball.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// The ball's path: one entry per frame, nullopt when no ball is seen.
struct Track {
    std::vector<Picture> pictures;

    // A ball away from walls and net (one dot of margin): its full 8 × 8 shows.
    static bool Clear(const Picture& picture) {
        if (!picture.box || !picture.ball) {
            return false;
        }
        const Box& box = *picture.box;
        const Ball& ball = *picture.ball;
        const int net = box.netColumn();
        return ball.left > box.left + kDot && ball.left + ball.width < box.right - kDot &&
               ball.top > box.top + 1 && ball.top + ball.height < box.bottom - 1 &&
               (ball.left + ball.width + kDot < net || ball.left > net + 3 * kDot);
    }

    struct Step {
        std::size_t frame;  // index of the second picture
        int dx, dy;
    };

    // Steps between consecutive frames whose balls are both clear.
    std::vector<Step> steps() const {
        std::vector<Step> result;
        for (std::size_t k = 1; k < pictures.size(); ++k) {
            if (Clear(pictures[k - 1]) && Clear(pictures[k])) {
                result.push_back({k, pictures[k].ball->left - pictures[k - 1].ball->left,
                                  pictures[k].ball->top - pictures[k - 1].ball->top});
            }
        }
        return result;
    }
};

Track Follow(LoadedCircuit& loaded, std::uint64_t seed, Time from, Time to) {
    Track track;
    Watch(loaded, seed, from, to,
          [&](const TvFrame& frame, bool) { track.pictures.push_back(Read(frame)); });
    return track;
}

int Sign(int value) { return (value > 0) - (value < 0); }

}  // namespace

TEST_CASE("ball: netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    CHECK(loaded->warnings.empty());
}

TEST_CASE("ball: box and dashed net", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2);
    auto loaded = BoardCircuit();
    std::optional<Picture> picture;
    bool locked = false;
    Watch(*loaded, seed, 400_ms, 445_ms, [&](const TvFrame& frame, bool isLocked) {
        picture = Read(frame);
        locked = isLocked;
    });
    REQUIRE(picture.has_value());
    CHECK(locked);
    REQUIRE(picture->box.has_value());
    const Box& box = *picture->box;
    CHECK(box.left - box.leftWall == kWallColumns);
    CHECK(box.rightWall - box.right == kWallColumns);
    CHECK(box.right - box.left + 1 == 88 * kDot);
    CHECK(box.leftWall == 96);  // 12 µs after the sync edge, as on 050
    // The TV draws the first line after vertical sync 32 columns early (free-running until the
    // next horizontal sync): line 0 is a full-width row, shifted left.
    CHECK(box.topWallRows == kWallRows);
    CHECK(box.bottomWallRows == kWallRows);
    CHECK(box.bottom - box.top + 1 == 224);  // lines 16–239
    // Dashed: 4 lines on, 4 off, on lines with V2 high (rows of line 16 + 4k, k odd, … ).
    REQUIRE(!picture->netRows.empty());
    CHECK(picture->netRows.size() == 112);
    for (const int row : picture->netRows) {
        CHECK(((row - picture->netRows.front()) % 8) < 4);
    }
    CHECK(picture->litOutside == 0);
}

namespace {

// One 2 × 8 ball moving 2 dots and 4 lines a frame, turning at each wall it meets.
void CheckBouncing(std::uint64_t seed) {
    auto loaded = BoardCircuit();
    const Track track = Follow(*loaded, seed, 100_ms, 2'000_ms);

    int clear = 0;
    for (const Picture& picture : track.pictures) {
        CHECK(picture.litOutside == 0);
        if (Track::Clear(picture)) {
            ++clear;
            CHECK(picture.ball->width == kBall);
            CHECK(picture.ball->height == kBall);
        }
    }
    CHECK(clear >= 20);

    const auto steps = track.steps();
    REQUIRE(steps.size() >= 10);
    for (const Track::Step& step : steps) {
        INFO("step " << step.dx << ", " << step.dy);
        CHECK(std::abs(step.dx) == 2 * kDot);
        CHECK(std::abs(step.dy) == 4);
    }

    // Each direction turns at least once in 1.9 s (a crossing takes about a second, and the ball
    // may start out of sight), and at the
    // wall it turns from: between the last step one way and the first step back, the ball's
    // visible part reaches that wall's inside edge.
    int horizontalTurns = 0, verticalTurns = 0;
    for (std::size_t i = 1; i < steps.size(); ++i) {
        const Track::Step& before = steps[i - 1];
        const Track::Step& after = steps[i];
        int minLeft = kTvColumns, maxRight = -1, minTop = kTvRows, maxBottom = -1;
        for (std::size_t k = before.frame; k < after.frame; ++k) {
            const Picture& picture = track.pictures[k];
            if (picture.ball) {
                minLeft = std::min(minLeft, picture.ball->left);
                maxRight = std::max(maxRight, picture.ball->left + picture.ball->width - 1);
                minTop = std::min(minTop, picture.ball->top);
                maxBottom = std::max(maxBottom, picture.ball->top + picture.ball->height - 1);
            }
        }
        const Box& box = *track.pictures[after.frame].box;
        INFO("frames " << before.frame << " to " << after.frame);
        if (Sign(before.dx) != Sign(after.dx)) {
            ++horizontalTurns;
            CHECK((before.dx > 0 ? maxRight == box.right : minLeft == box.left));
        }
        if (Sign(before.dy) != Sign(after.dy)) {
            ++verticalTurns;
            CHECK((before.dy > 0 ? maxBottom == box.bottom : minTop == box.top));
        }
    }
    CHECK(horizontalTurns >= 1);
    CHECK(verticalTurns >= 1);
}

}  // namespace

// One case per seed: ctest runs them in parallel (each simulates 2 s, about a minute in debug).
TEST_CASE("ball: one ball bouncing inside the box, seed 1", "[machines][slow]") {
    CheckBouncing(1);
}
TEST_CASE("ball: one ball bouncing inside the box, seed 2", "[machines][slow]") {
    CheckBouncing(2);
}
