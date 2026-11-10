#include <algorithm>
#include <cstdint>
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
#include "netlist/builder.hpp"
#include "parts/paddle.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

// Geometry (TENNIS.md, Circuit): one dot is 4 TV columns, one line one row. Dot 0 is column 96,
// 12 µs after the sync edge (as on 060); line 0 is the top wall's first row.
constexpr int kDot = 4;
constexpr int kDot0Column = 96;
constexpr int kDots = 96;            // active dots 0–95
constexpr int kCourtTop = 16;        // first line inside the court
constexpr int kCourtBottom = 239;    // last line inside the court
constexpr int kNetDot = 48;          // dots 48–49
constexpr int kLeftPaddleDot = 6;    // dots 6–7
constexpr int kRightPaddleDot = 88;  // dots 88–89

struct Ball {
    int left, top, width, height;  // dots and lines
};

struct PaddleSeen {
    int top, height;  // lines
};

struct Picture {
    bool walls = false;         // top wall lines 0–15 and bottom wall lines 240–255 found
    int lineZeroRow = 0;        // TV row of line 0
    std::vector<int> netLines;  // court lines with the net lit
    std::vector<int> litDots;   // per dot 0–95: court lines lit at that dot
    std::optional<Ball> ball;   // lit cells in the court that are not net, paddle or digit
    std::optional<PaddleSeen> left, right;
};

bool LitCell(const TvFrame& frame, int row, int dot) {
    const int column = kDot0Column + dot * kDot + kDot / 2;
    return row >= 0 && row < kTvRows && frame.at(row, column) >= 0.5f;
}

bool IsPaddleDot(int dot) {
    return dot == kLeftPaddleDot || dot == kLeftPaddleDot + 1 || dot == kRightPaddleDot ||
           dot == kRightPaddleDot + 1;
}

// Score digit cells: lines 32–63, dots 16–19, 24–27, 64–67, 72–75.
bool IsDigitCell(int line, int dot) {
    if (line < 32 || line > 63) {
        return false;
    }
    for (const int left : {16, 24, 64, 72}) {
        if (dot >= left && dot < left + 4) {
            return true;
        }
    }
    return false;
}

std::optional<PaddleSeen> ReadPaddle(const TvFrame& frame, int lineZeroRow, int dot) {
    int top = -1, bottom = -1;
    for (int line = kCourtTop; line <= kCourtBottom; ++line) {
        if (LitCell(frame, lineZeroRow + line, dot) &&
            LitCell(frame, lineZeroRow + line, dot + 1)) {
            if (top < 0) {
                top = line;
            }
            bottom = line;
        }
    }
    if (top < 0) {
        return std::nullopt;
    }
    return PaddleSeen{top, bottom - top + 1};
}

Picture Read(const TvFrame& frame) {
    Picture picture;
    // The top wall: the first run of rows lit across dots 1–94 (line 0 is drawn 32 columns
    // early, so test a row by its middle dots).
    std::vector<bool> full(kTvRows, false);
    for (int row = 0; row < kTvRows; ++row) {
        bool all = true;
        for (int dot = 1; dot < kDots - 1 && all; ++dot) {
            all = LitCell(frame, row, dot);
        }
        full[row] = all;
    }
    const auto first = std::ranges::find(full, true);
    if (first == full.end()) {
        return picture;
    }
    // Line 0 may not read as full (drawn early): the wall's first full row is line 0 or 1.
    int start = static_cast<int>(first - full.begin());
    int end = start;
    while (end + 1 < kTvRows && full[end + 1]) {
        ++end;
    }
    picture.lineZeroRow = end - 15;  // the top wall ends on line 15
    const int z = picture.lineZeroRow;
    bool walls = start <= z + 1;
    for (int line = 240; line <= 255 && walls; ++line) {
        walls = full[z + line];
    }
    walls = walls && !full[z + kCourtTop] && !full[z + kCourtBottom];
    picture.walls = walls;
    if (!walls) {
        return picture;
    }
    picture.litDots.assign(kDots, 0);
    int minLine = 1000, maxLine = -1, minDot = 1000, maxDot = -1;
    for (int line = kCourtTop; line <= kCourtBottom; ++line) {
        const int row = z + line;
        if (LitCell(frame, row, kNetDot) && LitCell(frame, row, kNetDot + 1)) {
            picture.netLines.push_back(line);
        }
        for (int dot = 0; dot < kDots; ++dot) {
            if (!LitCell(frame, row, dot)) {
                continue;
            }
            ++picture.litDots[dot];
            if (dot == kNetDot || dot == kNetDot + 1 || IsPaddleDot(dot) ||
                IsDigitCell(line, dot)) {
                continue;
            }
            minLine = std::min(minLine, line);
            maxLine = std::max(maxLine, line);
            minDot = std::min(minDot, dot);
            maxDot = std::max(maxDot, dot);
        }
    }
    if (maxLine >= 0) {
        picture.ball = Ball{minDot, minLine, maxDot - minDot + 1, maxLine - minLine + 1};
    }
    picture.left = ReadPaddle(frame, z, kLeftPaddleDot);
    picture.right = ReadPaddle(frame, z, kRightPaddleDot);
    return picture;
}

// Called before each 1 ms step: knobs, the Start button.
using StepHook = std::function<void(Simulator&)>;
// Called for each completed frame that started at or after `from`.
using FrameHook = std::function<void(const TvFrame&, bool locked, Simulator&)>;

// Runs the TV from power-on in 1 ms steps (Tv keeps only the last completed frame).
void Watch(LoadedCircuit& loaded, std::uint64_t seed, Time from, Time to, const FrameHook& onFrame,
           const StepHook& onStep = {}) {
    Simulator sim(loaded.circuit);
    sim.powerOn(seed);
    TvInput tv(sim, *ConnectorNet(loaded, "VIDEO", "VIDEO"),
               *ConnectorNet(loaded, "VIDEO", "SYNC_N"));
    std::uint64_t frames = 0;
    while (sim.now() < to) {
        if (onStep) {
            onStep(sim);
        }
        sim.runFor(1_ms);
        tv.update();
        sim.trimRecordings();
        if (tv.tv().completedFrames() != frames) {
            frames = tv.tv().completedFrames();
            if (tv.tv().completedStats().start >= from) {
                onFrame(tv.tv().completedFrame(), tv.tv().isLocked(), sim);
            }
        }
    }
}

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "tennis.bwn");
    REQUIRE(result.has_value());
    return std::move(*result);
}

}  // namespace

TEST_CASE("tennis: netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    for (const auto& warning : loaded->warnings) {
        UNSCOPED_INFO(warning.line << ": " << warning.message);
    }
    CHECK(loaded->warnings.empty());
}

TEST_CASE("tennis: court", "[machines][slow]") {
    auto loaded = DesignCircuit();
    int frames = 0, unlocked = 0, wallsMissing = 0;
    int edgeDots = 0;  // lit cells on dots 0–1 and 94–95 inside the court
    bool leftTurn = false, rightTurn = false;
    bool inside = false;
    std::vector<Picture> pictures;
    Watch(*loaded, 1, 300_ms, 2'300_ms, [&](const TvFrame& frame, bool locked, Simulator&) {
        ++frames;
        unlocked += locked ? 0 : 1;
        const Picture picture = Read(frame);
        if (!picture.walls) {
            ++wallsMissing;
            return;
        }
        // At power-on the ball may come in from outside the picture, across the edge dots.
        inside = inside || (picture.ball && picture.ball->left >= 4 && picture.ball->left <= 90);
        for (const int dot : {0, 1, 94, 95}) {
            edgeDots += inside ? picture.litDots[dot] : 0;
        }
        if (pictures.empty()) {
            // Dashed: 4 lines on, 4 off.
            CHECK(picture.netLines.size() == 112);
            for (const int line : picture.netLines) {
                CHECK(((line - picture.netLines.front()) % 8) < 4);
            }
        }
        pictures.push_back(picture);
    });
    CHECK(frames >= 95);
    CHECK(unlocked == 0);
    CHECK(wallsMissing == 0);
    CHECK(edgeDots == 0);
    // The ball turns at dots 2–3 and 92–93, seen there (the goals are not drawn).
    for (std::size_t k = 1; k + 1 < pictures.size(); ++k) {
        const auto& before = pictures[k - 1].ball;
        const auto& at = pictures[k].ball;
        const auto& after = pictures[k + 1].ball;
        if (!before || !at || !after || at->width != 2) {
            continue;
        }
        if (at->left == 2 && before->left == 4 && after->left == 4) {
            leftTurn = true;
        }
        if (at->left == 92 && before->left == 90 && after->left == 90) {
            rightTurn = true;
        }
    }
    CHECK(leftTurn);
    CHECK(rightTurn);
}

namespace {

// The knob that puts a paddle's top on `line` (TENNIS.md: top = ⌊224.5 − 208 k⌋).
double KnobForTop(int line) { return std::clamp((224.0 - line) / 208.0, 0.0, 1.0); }

bool ClearOf(const Picture& picture, int paddleDot) {
    return picture.ball && std::abs(picture.ball->left - paddleDot) > 2;
}

}  // namespace

TEST_CASE("tennis: paddles follow the knobs", "[machines]") {
    auto loaded = DesignCircuit();
    Paddle* left = PaddleOf(*loaded, "J3");
    Paddle* right = PaddleOf(*loaded, "J4");
    REQUIRE(left != nullptr);
    REQUIRE(right != nullptr);
    struct Step {
        Time at;
        double knob;
        int top;  // left paddle; the right one is set to 1 − knob
    };
    const std::vector<Step> steps{{0, 0.0, 224}, {120_ms, 0.5, 120}, {240_ms, 1.0, 16}};
    for (const Step& step : steps) {
        CHECK(KnobForTop(step.top) == step.knob);
    }
    std::vector<int> checked(steps.size(), 0);
    Watch(
        *loaded, 1, 40_ms, 360_ms,
        [&](const TvFrame& frame, bool, Simulator& sim) {
            // Frames that started at least two frames after a turn show it.
            const Time start = sim.now() - 1_ms;
            for (std::size_t i = 0; i < steps.size(); ++i) {
                const Time end = i + 1 < steps.size() ? steps[i + 1].at : 360_ms;
                if (start < steps[i].at + 45_ms || start >= end) {
                    continue;
                }
                const Picture picture = Read(frame);
                REQUIRE(picture.walls);
                INFO("knob " << steps[i].knob);
                if (ClearOf(picture, kLeftPaddleDot)) {
                    REQUIRE(picture.left.has_value());
                    CHECK(picture.left->top == steps[i].top);
                    CHECK(picture.left->height == 16);
                    ++checked[i];
                }
                if (ClearOf(picture, kRightPaddleDot)) {
                    REQUIRE(picture.right.has_value());
                    CHECK(picture.right->top == 224 + 16 - steps[i].top);
                    CHECK(picture.right->height == 16);
                }
            }
        },
        [&](Simulator& sim) {
            for (const Step& step : steps) {
                if (sim.now() == step.at) {
                    left->setKnob(step.knob);
                    right->setKnob(1.0 - step.knob);
                }
            }
        });
    for (const int count : checked) {
        CHECK(count >= 2);
    }
}
