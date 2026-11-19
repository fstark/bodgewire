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

#include "../machine/test_points.hpp"
#include "../paths.hpp"
#include "devices/tv_input.hpp"
#include "instruments/logic_probe.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"
#include "netlist/builder.hpp"
#include "sim/simulator.hpp"
#include "tv_window.hpp"

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
    auto result = LoadNetlistFile(test::NetlistPath("ball"));
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Circuit built from the Board's Copper, with no Faults.
std::unique_ptr<LoadedCircuit> BoardCircuit() {
    auto machine = LoadNetlistMachine(test::NetlistPath("ball"));
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
    std::optional<LitBox> lit;
    bool locked = false;
    Watch(*loaded, seed, 400_ms, 445_ms, [&](const TvFrame& frame, bool isLocked) {
        picture = Read(frame);
        lit = LitBounds(frame);
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
    // The vertical sync on lines 280-283 (P6n): the frame starts 16 µs into line 280, line 0 on
    // row 32, the top wall's first row; the box inside a monitor's window.
    CHECK(box.top - box.topWallRows == 32);
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
    REQUIRE(lit.has_value());
    CHECK(InsideWindow(*lit, 312));
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

namespace {

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(test::PuzzlePath(name));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

std::unique_ptr<LoadedCircuit> Build(const Machine& machine) {
    auto built = machine.build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// Lit columns from the net's first column rightwards, on each row the net is drawn.
std::vector<int> NetWidths(const TvFrame& frame, const Picture& picture) {
    std::vector<int> widths;
    const int net = picture.box->netColumn();
    for (const int row : picture.netRows) {
        int column = net;
        while (column <= picture.box->right && Lit(frame, row, column)) {
            ++column;
        }
        widths.push_back(column - net);
    }
    return widths;
}

// The widest the net is drawn on most of its rows, in one frame at 445 ms.
int NetWidth(LoadedCircuit& loaded, std::vector<std::string>& lamps,
             const std::vector<std::string>& probes) {
    std::vector<int> widths;
    lamps = Watch(
        loaded, 1, 400_ms, 445_ms,
        [&](const TvFrame& frame, bool) {
            const Picture picture = Read(frame);
            REQUIRE(picture.box.has_value());
            widths = NetWidths(frame, picture);
        },
        probes);
    REQUIRE(widths.size() >= 100);
    std::sort(widths.begin(), widths.end());
    return widths[widths.size() / 2];
}

// Rows the net is drawn on, in one frame at 445 ms.
std::size_t NetRows(LoadedCircuit& loaded, std::vector<std::string>& lamps,
                    const std::vector<std::string>& probes) {
    std::size_t rows = 0;
    lamps = Watch(
        loaded, 1, 400_ms, 445_ms,
        [&](const TvFrame& frame, bool) {
            const Picture picture = Read(frame);
            REQUIRE(picture.box.has_value());
            rows = picture.netRows.size();
        },
        probes);
    return rows;
}

bool Pulses(const std::string& lamps) { return lamps.find("PULSE") != std::string::npos; }

}  // namespace

TEST_CASE("ball: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : {"solid_net", "wraparound", "stripe", "jumping_ball", "net_bounce"}) {
        INFO(name);
        const auto puzzle = ReadPuzzleFile(test::PuzzlePath(name));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "ball");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("ball: solid_net", "[machines]") {
    Machine machine = LoadPuzzle("solid_net");
    std::vector<std::string> lamps;
    const Point pin13 = *machine.padPosition("U15", "13");
    {
        auto loaded = Build(machine);
        const NetId v2 = *PinNet(*loaded, "U3", "QC");
        CHECK(machine.probeNet(*loaded, Layer::Back, pin13) == v2);  // the pad
        CHECK(machine.probeNet(*loaded, Layer::Front, pin13) == PinNet(*loaded, "U15", "2D"));
        CHECK(*PinNet(*loaded, "U15", "2D") != v2);  // the leg
        // V2 floats high at the gate: the net is drawn on every line of the box.
        CHECK(NetRows(*loaded, lamps, {"U15.13", "U3.QC"}) == 224);
        CHECK(lamps[0] == "-");
        CHECK(Pulses(lamps[1]));
    }
    REQUIRE(machine.swap("U15"));
    {
        auto swapped = Build(machine);  // a swap keeps the Joint
        CHECK(*PinNet(*swapped, "U15", "2D") != *PinNet(*swapped, "U3", "QC"));
    }
    REQUIRE(machine.resolder({"U15", "13"}));
    auto loaded = Build(machine);
    CHECK(NetRows(*loaded, lamps, {}) == 112);
    CHECK(NetWidth(*loaded, lamps, {}) == 2 * kDot);
}

namespace {

// The tallest ball seen between 400 and 600 ms.
int TallestBall(LoadedCircuit& loaded, std::vector<std::string>& lamps,
                const std::vector<std::string>& probes) {
    int tallest = 0;
    lamps = Watch(
        loaded, 1, 400_ms, 600_ms,
        [&](const TvFrame& frame, bool) {
            const Picture picture = Read(frame);
            if (picture.ball) {
                tallest = std::max(tallest, picture.ball->height);
            }
        },
        probes);
    return tallest;
}

}  // namespace

TEST_CASE("ball: stripe", "[machines]") {
    Machine machine = LoadPuzzle("stripe");
    std::vector<std::string> lamps;
    {
        auto loaded = Build(machine);
        // A bar the whole height of the box (lines 16–239).
        CHECK(TallestBall(*loaded, lamps, {"U23.QA", "U23.CKB"}) == 224);
        CHECK(lamps[0] == "LOW");
        CHECK(Pulses(lamps[1]));
    }
    REQUIRE(machine.swap("U23"));
    auto loaded = Build(machine);
    CHECK(TallestBall(*loaded, lamps, {}) == kBall);
}

TEST_CASE("ball: jumping_ball", "[machines]") {
    Machine machine = LoadPuzzle("jumping_ball");
    {
        auto loaded = Build(machine);
        REQUIRE(*PinNet(*loaded, "U20", "LOAD_N") != *PinNet(*loaded, "U24", "2Y"));
        Track track;
        const auto lamps =
            Watch(*loaded, 1, 400_ms, 700_ms,
                  [&](const TvFrame& frame, bool) { track.pictures.push_back(Read(frame)); },
                  {"U24.2Y", "U20.LOAD_N", "U21.LOAD_N"});
        // Drawn once every 512 lines: in one frame of two or three, 88 lines lower or 112 higher
        // than where it last showed.
        int jumps = 0, missing = 0;
        std::optional<Ball> last;
        for (const Picture& picture : track.pictures) {
            if (!picture.ball) {
                ++missing;
                continue;
            }
            if (last && std::abs(picture.ball->top - last->top) > 8) {
                ++jumps;
            }
            last = picture.ball;
        }
        CHECK(jumps >= 3);
        CHECK(missing >= 3);
        CHECK(Pulses(lamps[0]));
        CHECK(lamps[1] == "-");
        CHECK(lamps[2] == "-");
    }
    REQUIRE(machine.mend(JumpingBallCrack().layer, JumpingBallCrack().at));
    auto loaded = Build(machine);
    const Track track = Follow(*loaded, 1, 400_ms, 700_ms);
    const auto steps = track.steps();
    CHECK(steps.size() >= 3);
    for (const Track::Step& step : steps) {
        CHECK(std::abs(step.dy) == 4);
    }
}

TEST_CASE("ball: wraparound", "[machines][slow]") {
    auto loaded = Build(LoadPuzzle("wraparound"));
    Track track;
    const auto lamps = Watch(
        *loaded, 1, 100_ms, 2'000_ms,
        [&](const TvFrame& frame, bool) { track.pictures.push_back(Read(frame)); }, {"U26.1Q"});
    CHECK(lamps[0] == "HIGH");
    const auto steps = track.steps();
    REQUIRE(steps.size() >= 10);
    for (const Track::Step& step : steps) {
        CHECK(step.dx == 2 * kDot);
    }
    // It reaches the right wall and later comes back in at the left one.
    std::optional<std::size_t> right;
    bool backLeft = false;
    for (std::size_t k = 0; k < track.pictures.size(); ++k) {
        const Picture& picture = track.pictures[k];
        if (!picture.ball || !picture.box) {
            continue;
        }
        if (picture.ball->left + picture.ball->width - 1 == picture.box->right && !right) {
            right = k;
        }
        if (right && picture.ball->left == picture.box->left) {
            backLeft = true;
        }
    }
    CHECK(right.has_value());
    CHECK(backLeft);
}

TEST_CASE("ball: wraparound, U26 swapped", "[machines][slow]") {
    Machine machine = LoadPuzzle("wraparound");
    REQUIRE(machine.swap("U26"));
    auto loaded = Build(machine);
    const auto steps = Follow(*loaded, 1, 100_ms, 2'000_ms).steps();
    CHECK(std::ranges::any_of(steps, [](const Track::Step& step) { return step.dx < 0; }));
    CHECK(std::ranges::any_of(steps, [](const Track::Step& step) { return step.dx > 0; }));
}

namespace {

// Frames, from the first with the ball left of the net, in which the ball is right of it.
int RightOfNetAfterLeft(LoadedCircuit& loaded) {
    const Track track = Follow(loaded, 1, 100_ms, 2'000_ms);
    bool seenLeft = false;
    int right = 0;
    for (const Picture& picture : track.pictures) {
        if (!picture.ball || !picture.box) {
            continue;
        }
        const int net = picture.box->netColumn();
        if (picture.ball->left + picture.ball->width <= net) {
            seenLeft = true;
        } else if (seenLeft && picture.ball->left >= net + 2 * kDot) {
            ++right;
        }
    }
    CHECK(seenLeft);
    return right;
}

}  // namespace

TEST_CASE("ball: net_bounce", "[machines][slow]") {
    auto loaded = Build(LoadPuzzle("net_bounce"));
    REQUIRE(*PinNet(*loaded, "U16", "D") == *PinNet(*loaded, "U16", "E"));
    CHECK(RightOfNetAfterLeft(*loaded) == 0);
}

TEST_CASE("ball: net_bounce, bridge cut", "[machines][slow]") {
    Machine machine = LoadPuzzle("net_bounce");
    REQUIRE(machine.removeBridge({"U16", "4"}, {"U16", "5"}));
    auto loaded = Build(machine);
    CHECK(RightOfNetAfterLeft(*loaded) > 0);
}
