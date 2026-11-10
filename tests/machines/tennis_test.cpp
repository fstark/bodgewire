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
    Watch(*loaded, 1, 300_ms, 4'300_ms, [&](const TvFrame& frame, bool locked, Simulator&) {
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
    CHECK(frames >= 195);
    CHECK(unlocked == 0);
    CHECK(wallsMissing == 0);
    CHECK(edgeDots == 0);
    // The ball reaches dots 2–3 and 92–93, seen there (the goals are not drawn).
    for (const Picture& picture : pictures) {
        if (picture.ball && picture.ball->width == 2) {
            leftTurn = leftTurn || picture.ball->left == 2;
            rightTurn = rightTurn || picture.ball->left == 92;
        }
    }
    CHECK(leftTurn);
    CHECK(rightTurn);
}

namespace {

// The knob that puts a paddle's top on `line` (TENNIS.md: top = ⌊224.5 − 208 k⌋).
double KnobForTop(int line) { return std::clamp((224.0 - line) / 208.0, 0.0, 1.0); }

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
    const std::vector<Step> steps{{0, 0.0, 224}, {200_ms, 0.5, 120}, {400_ms, 1.0, 16}};
    for (const Step& step : steps) {
        CHECK(KnobForTop(step.top) == step.knob);
    }
    std::vector<int> checked(steps.size(), 0);
    Watch(
        *loaded, 1, 40_ms, 600_ms,
        [&](const TvFrame& frame, bool, Simulator& sim) {
            // Frames that started at least two frames after a turn show it.
            const Time start = sim.now() - 1_ms;
            for (std::size_t i = 0; i < steps.size(); ++i) {
                const Time end = i + 1 < steps.size() ? steps[i + 1].at : 600_ms;
                if (start < steps[i].at + 45_ms || start >= end) {
                    continue;
                }
                const Picture picture = Read(frame);
                REQUIRE(picture.walls);
                INFO("knob " << steps[i].knob);
                // A paddle seen 16 lines tall (not lengthened by the ball on its column).
                REQUIRE(picture.left.has_value());
                REQUIRE(picture.right.has_value());
                if (picture.left->height == 16) {
                    CHECK(picture.left->top == steps[i].top);
                    ++checked[i];
                }
                if (picture.right->height == 16) {
                    CHECK(picture.right->top == 224 + 16 - steps[i].top);
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

namespace {

// A ball fully seen (2 dots wide, 8 lines tall, away from the walls).
std::optional<Ball> Seen(const Picture& picture) {
    if (!picture.walls || !picture.ball || picture.ball->width != 2 || picture.ball->height != 8) {
        return std::nullopt;
    }
    return picture.ball;
}

// A paddle hit as the pictures show it: the ball one step from the paddle in frame `before`,
// unseen on the paddle's column in the next, back at the same step after.
struct Contact {
    bool left;        // the left paddle
    int paddleTop;    // from frame `before`
    int lineFour;     // the ball's line 4 at the contact, on the paddle's lines (0–15 on it)
    int dyIn, dyOut;  // lines a frame before; after, from the 1st to the 2nd frame after the
                      // contact (the ball's reload is on its first line, before line 4)
};

// Plays both paddles: each frame, sets the knobs so that the ball's line 4 will meet each paddle
// on its line `aim`, from where the ball will be when it gets there; holds a knob still in the
// last three frames (a knob shows two frames after it is set). Follows the ball while it moves
// away.
class Player {
public:
    Player(Paddle& left, Paddle& right, int aim) : left_(left), right_(right), aim_(aim) {}

    void onFrame(const Picture& picture) {
        const std::optional<Ball> ball = Seen(picture);
        history_.push_back(ball);
        if (ball && picture.left && picture.right) {
            paddleTops_.push_back({picture.left->top, picture.right->top});
        } else {
            paddleTops_.push_back(paddleTops_.empty() ? std::pair{-1, -1} : paddleTops_.back());
        }
        // The ball's speed over the last four frames (sideways 1 or 2 dots a frame).
        const std::size_t n = history_.size();
        if (n < 5 || !history_[n - 1] || !history_[n - 5]) {
            return;
        }
        const Ball& now = *history_[n - 1];
        const double dx = (now.left - history_[n - 5]->left) / 4.0;
        if (!history_[n - 2] || dx == 0) {
            return;
        }
        const int dy = now.top - history_[n - 2]->top;
        // Frames until the ball is on each paddle's column; a knob shows two frames after it is
        // set, so it is held from three frames before.
        const double toLeft = dx < 0 ? (now.left - kLeftPaddleDot) / -dx : -1;
        const double toRight = dx > 0 ? (kRightPaddleDot - now.left) / dx : -1;
        aim(left_, now, dy, toLeft);
        aim(right_, now, dy, toRight);
    }

    // Fast contacts only (the ball moved 2 dots in each of the two frames before), away from
    // the walls.
    std::vector<Contact> contacts() const {
        std::vector<Contact> result;
        for (std::size_t f = 2; f + 3 < history_.size(); ++f) {
            const auto& b0 = history_[f - 2];
            const auto& b1 = history_[f - 1];
            const auto& b2 = history_[f];
            const auto& after = history_[f + 2];
            const auto& later = history_[f + 3];
            if (!b0 || !b1 || !b2 || history_[f + 1] || !after || !later) {
                continue;
            }
            const bool left = b2->left == kLeftPaddleDot + 2 && b1->left == b2->left + 2 &&
                              b0->left == b1->left + 2 && after->left == b2->left;
            const bool right = b2->left == kRightPaddleDot - 2 && b1->left == b2->left - 2 &&
                               b0->left == b1->left - 2 && after->left == b2->left;
            if (!left && !right) {
                continue;
            }
            const int dyIn = b2->top - b1->top;
            const int contactTop = b2->top + dyIn;
            if (b2->top < kCourtTop + 8 || b2->top > kCourtBottom - 16 ||
                later->top < kCourtTop + 4 || later->top > kCourtBottom - 12) {
                continue;
            }
            const int paddleTop = left ? paddleTops_[f].first : paddleTops_[f].second;
            result.push_back(
                {left, paddleTop, contactTop + 4 - paddleTop, dyIn, later->top - after->top});
        }
        return result;
    }

    // Turns of the ball at a paddle: its sideways direction reverses with the ball never past
    // the paddle's neighbouring step.
    int returns() const {
        int count = 0, direction = 0, lastLeft = -1;
        for (const auto& ball : history_) {
            if (!ball || ball->left == lastLeft) {
                continue;
            }
            const int now = lastLeft < 0 ? 0 : (ball->left > lastLeft ? 1 : -1);
            if (direction != 0 && now != 0 && now != direction &&
                ((now > 0 && lastLeft == kLeftPaddleDot + 2) ||
                 (now < 0 && lastLeft == kRightPaddleDot - 2))) {
                ++count;
            }
            direction = now != 0 ? now : direction;
            lastLeft = ball->left;
        }
        return count;
    }

    // Frames in which the ball was seen behind a paddle (between it and its goal).
    int behind() const {
        int count = 0;
        bool hit = false;
        for (const auto& ball : history_) {
            if (!ball) {
                continue;
            }
            hit = hit || ball->left == kLeftPaddleDot + 2 || ball->left == kRightPaddleDot - 2;
            if (hit && (ball->left < kLeftPaddleDot || ball->left > kRightPaddleDot)) {
                ++count;
            }
        }
        return count;
    }

private:
    // The ball's top `frames` from now, bouncing off the walls (approximately).
    static int TopAfter(const Ball& ball, int dy, double frames) {
        int top = ball.top + static_cast<int>(dy * frames);
        constexpr int kHigh = kCourtTop, kLow = kCourtBottom - 7;
        for (int i = 0; i < 4 && (top < kHigh || top > kLow); ++i) {
            top = top < kHigh ? 2 * kHigh - top : 2 * kLow - top;
        }
        return top;
    }

    void aim(Paddle& paddle, const Ball& ball, int dy, double frames) {
        if (frames < 0) {
            paddle.setKnob(KnobForTop(ball.top + 4 - aim_));  // moving away: follow
        } else if (frames >= 3) {
            paddle.setKnob(KnobForTop(TopAfter(ball, dy, frames) + 4 - aim_));
        }
    }

    Paddle& left_;
    Paddle& right_;
    int aim_;
    std::vector<std::optional<Ball>> history_;
    std::vector<std::pair<int, int>> paddleTops_;
};

// The vertical speed a hit on paddle line `line` gives (TENNIS.md, Angles).
int SpeedFor(int line) {
    constexpr int kSpeeds[] = {-4, -2, 2, 4};
    return kSpeeds[line / 4];
}

}  // namespace

namespace {

// Both players aim at paddle line `aim` for 4 s.
Player Rally(int aim) {
    auto loaded = DesignCircuit();
    Player player(*PaddleOf(*loaded, "J3"), *PaddleOf(*loaded, "J4"), aim);
    Watch(*loaded, 1, 40_ms, 4'000_ms,
          [&](const TvFrame& frame, bool, Simulator&) { player.onFrame(Read(frame)); });
    return player;
}

// Every fast contact leaves at the speed of the quarter hit; at least two in the aimed one.
void CheckAngles(int aim) {
    const Player player = Rally(aim);
    int aimed = 0;
    for (const Contact& contact : player.contacts()) {
        INFO((contact.left ? "left" : "right")
             << " paddle top " << contact.paddleTop << ", line 4 at " << contact.lineFour << ", dy "
             << contact.dyIn << " -> " << contact.dyOut);
        if (contact.lineFour < 0 || contact.lineFour > 15) {
            CHECK(contact.dyOut == contact.dyIn);  // line 4 off the paddle: no new angle
            continue;
        }
        CHECK(contact.dyOut == SpeedFor(contact.lineFour));
        aimed += contact.lineFour / 4 == aim / 4 ? 1 : 0;
    }
    CHECK(aimed >= 2);
}

}  // namespace

TEST_CASE("tennis: the paddles return the ball", "[machines][slow]") {
    const Player player = Rally(8);  // the middle of the paddle
    CHECK(player.behind() == 0);
    CHECK(player.returns() >= 3);
}

TEST_CASE("tennis: angle, top quarter", "[machines][slow]") { CheckAngles(2); }
TEST_CASE("tennis: angle, upper middle quarter", "[machines][slow]") { CheckAngles(6); }
TEST_CASE("tennis: angle, lower middle quarter", "[machines][slow]") { CheckAngles(9); }
TEST_CASE("tennis: angle, bottom quarter", "[machines][slow]") { CheckAngles(13); }
