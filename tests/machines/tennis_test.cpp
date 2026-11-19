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

#include "../machine/test_points.hpp"
#include "../paths.hpp"
#include "devices/tv_input.hpp"
#include "instruments/logic_probe.hpp"
#include "machine/machine.hpp"
#include "machine/puzzle.hpp"
#include "netlist/builder.hpp"
#include "parts/ls47.hpp"
#include "parts/paddle.hpp"
#include "sim/simulator.hpp"
#include "tv_window.hpp"

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
    int leftScore = -1, rightScore = -1;  // 0–19 as drawn; -1 a shape that is no digit
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

// The digit drawn in the cell whose left dot is `dot` (lines 32–63): the segments sampled at
// one point each (a, g, d on the cell's dot 1 at lines 33, 45, 61; f, b on dots 0, 3 at line 39;
// e, c at line 53), matched against the 74LS48 glyphs 0–9; 15 is blank (0x00). -1 otherwise.
int ReadDigit(const TvFrame& frame, int lineZeroRow, int dot) {
    const auto lit = [&](int line, int at) { return LitCell(frame, lineZeroRow + line, dot + at); };
    const std::uint8_t segments =
        static_cast<std::uint8_t>(lit(33, 1) | lit(39, 3) << 1 | lit(53, 3) << 2 | lit(61, 1) << 3 |
                                  lit(53, 0) << 4 | lit(39, 0) << 5 | lit(45, 1) << 6);
    for (unsigned value = 0; value < 10; ++value) {
        if (Ls47Segments(value, true, true, true) == segments) {
            return static_cast<int>(value);
        }
    }
    return -1;
}

// A score: the units digit, and the tens cell lit on its column 3 ("1") or dark.
int ReadScore(const TvFrame& frame, int lineZeroRow, int tensDot) {
    const int units = ReadDigit(frame, lineZeroRow, tensDot + 8);
    bool one = true, dark = true;
    for (int line = 32; line < 64; ++line) {
        for (int at = 0; at < 4; ++at) {
            const bool on = LitCell(frame, lineZeroRow + line, tensDot + at);
            one = one && on == (at == 3);
            dark = dark && !on;
        }
    }
    if (units < 0 || (!one && !dark)) {
        return -1;
    }
    return (one ? 10 : 0) + units;
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
    if (z < 0 || z + 255 >= kTvRows) {  // a rolling picture (the TV not locked yet)
        return picture;
    }
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
    picture.leftScore = ReadScore(frame, z, 16);
    picture.rightScore = ReadScore(frame, z, 64);
    picture.left = ReadPaddle(frame, z, kLeftPaddleDot);
    picture.right = ReadPaddle(frame, z, kRightPaddleDot);
    return picture;
}

// Called before each 1 ms step: knobs, the Start button.
using StepHook = std::function<void(Simulator&)>;
// Called for each completed frame that started at or after `from`.
using FrameHook = std::function<void(const TvFrame&, bool locked, Simulator&)>;

// Runs the TV from power-on in 1 ms steps (Tv keeps only the last completed frame). Returns the
// lamps of a logic probe on each of `probes` (REF.PIN, the pin's own Net) over the whole run.
std::vector<std::string> Watch(LoadedCircuit& loaded, std::uint64_t seed, Time from, Time to,
                               const FrameHook& onFrame, const StepHook& onStep = {},
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
        if (onStep) {
            onStep(sim);
        }
        sim.runFor(1_ms);
        tv.update();
        for (LogicProbe& probe : lamps) {
            probe.update();
        }
        sim.trimRecordings();
        if (tv.tv().completedFrames() != frames) {
            frames = tv.tv().completedFrames();
            if (tv.tv().completedStats().start >= from) {
                onFrame(tv.tv().completedFrame(), tv.tv().isLocked(), sim);
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
    auto result = LoadNetlistFile(test::NetlistPath("tennis"));
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Circuit built from the Board's Copper, with no Faults.
std::unique_ptr<LoadedCircuit> BoardCircuit() {
    auto machine = LoadNetlistMachine(test::NetlistPath("tennis"));
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// Presses Start for 2 ms at `at`.
StepHook PressStart(LoadedCircuit& loaded, Time at) {
    Switch* button = SwitchOf(loaded, "SW1");
    REQUIRE(button != nullptr);
    return [button, at](Simulator& sim) {
        if (sim.now() == at || sim.now() == at + 2_ms) {
            button->setPressed(sim.now() == at);
            sim.wake(button->id());
        }
    };
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
    auto loaded = BoardCircuit();
    int frames = 0, unlocked = 0, wallsMissing = 0;
    int edgeDots = 0;       // lit cells on dots 0–1 and 94–95 inside the court
    int outsideWindow = 0;  // frames lit outside a monitor's window (P6n)
    bool inside = false;
    std::vector<Picture> pictures;
    Watch(*loaded, 1, 300_ms, 4'300_ms, [&](const TvFrame& frame, bool locked, Simulator&) {
        ++frames;
        unlocked += locked ? 0 : 1;
        const auto lit = LitBounds(frame);
        outsideWindow += lit && !InsideWindow(*lit, 312) ? 1 : 0;
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
    CHECK(outsideWindow == 0);
}

namespace {

// The knob that puts a paddle's top on `line` (TENNIS.md: top = ⌊224.5 − 208 k⌋).
double KnobForTop(int line) { return std::clamp((224.0 - line) / 208.0, 0.0, 1.0); }

}  // namespace

TEST_CASE("tennis: paddles follow the knobs", "[machines]") {
    auto loaded = BoardCircuit();
    Paddle* left = PaddleOf(*loaded, "J3");
    Paddle* right = PaddleOf(*loaded, "J4");
    REQUIRE(left != nullptr);
    REQUIRE(right != nullptr);
    struct Step {
        Time at;
        double knob;
        int top;  // left paddle; the right one is set to 1 − knob
    };
    // The TV may take half a second to lock.
    const std::vector<Step> steps{{0, 0.0, 224}, {700_ms, 0.5, 120}, {900_ms, 1.0, 16}};
    for (const Step& step : steps) {
        CHECK(KnobForTop(step.top) == step.knob);
    }
    std::vector<int> checked(steps.size(), 0);
    Watch(
        *loaded, 1, 40_ms, 1'100_ms,
        [&](const TvFrame& frame, bool, Simulator& sim) {
            // Frames that started at least two frames after a turn show it.
            const Time start = sim.now() - 1_ms;
            for (std::size_t i = 0; i < steps.size(); ++i) {
                const Time end = i + 1 < steps.size() ? steps[i + 1].at : 1'100_ms;
                if (start < std::max(steps[i].at + 45_ms, 550_ms) || start >= end) {
                    continue;
                }
                const Picture picture = Read(frame);
                INFO("frame ending " << sim.now() / 1'000'000'000 << " ms");
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
        [&, start = PressStart(*loaded, 1_ms)](Simulator& sim) {
            start(sim);  // a power-on may be game over, with the paddles hidden
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
    // `misses`: 0 none, -1 the left player keeps away from the ball, +1 the right player.
    Player(Paddle& left, Paddle& right, int aim, int misses = 0)
        : left_(left), right_(right), aim_(aim), misses_(misses) {}

    const std::vector<std::optional<Ball>>& history() const { return history_; }
    void stopMissing() { misses_ = 0; }

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
        aim(left_, now, dy, toLeft, misses_ < 0);
        aim(right_, now, dy, toRight, misses_ > 0);
    }

    // Contacts away from the walls: the ball one step from a paddle in frame f, unseen on the
    // paddle's column for n frames (1 fast, 2 slow), back at that step. The ball's line 4 at the
    // last contact frame is worked out from frame f (it moves dyIn a frame until a new angle
    // shows); the new angle shows from the 1st to the 2nd frame after the last contact (the
    // ball's reload is on its first line, before line 4).
    std::vector<Contact> contacts() const {
        std::vector<Contact> result;
        for (std::size_t f = 1; f + 4 < history_.size(); ++f) {
            const auto& b1 = history_[f - 1];
            const auto& b2 = history_[f];
            if (!b1 || !b2 || history_[f + 1]) {
                continue;
            }
            const std::size_t n = history_[f + 2] ? 1 : 2;
            const auto& after = history_[f + n + 1];
            const auto& later = history_[f + n + 2];
            if (!after || !later) {
                continue;
            }
            const bool left =
                b2->left == kLeftPaddleDot + 2 && after->left == b2->left && b1->left >= b2->left;
            const bool right =
                b2->left == kRightPaddleDot - 2 && after->left == b2->left && b1->left <= b2->left;
            if (!left && !right) {
                continue;
            }
            const int dyIn = b2->top - b1->top;
            const int contactTop = b2->top + static_cast<int>(n) * dyIn;
            if (b1->top < kCourtTop + 12 || b2->top > kCourtBottom - 20 ||
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

    void aim(Paddle& paddle, const Ball& ball, int dy, double frames, bool away) {
        if (away) {  // the far end of the court from where the ball will be
            const int top = TopAfter(ball, dy, std::max(frames, 0.0));
            paddle.setKnob(top < 128 ? 0.0 : 1.0);
        } else if (frames < 0) {
            paddle.setKnob(KnobForTop(ball.top + 4 - aim_));  // moving away: follow
        } else if (frames >= 3) {
            paddle.setKnob(KnobForTop(TopAfter(ball, dy, frames) + 4 - aim_));
        }
    }

    Paddle& left_;
    Paddle& right_;
    int aim_;
    int misses_;
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

// The first seed from 1 whose power-on state (1 µs in) passes `wanted`. Power-on draws a bit for
// every output and Net, so what a seed gives changes with the netlist: search, never hard-code.
std::uint64_t FindSeed(LoadedCircuit& loaded, const std::function<bool(const Simulator&)>& wanted) {
    for (std::uint64_t seed = 1; seed < 200; ++seed) {
        Simulator sim(loaded.circuit);
        sim.powerOn(seed);
        sim.runFor(1_us);
        if (wanted(sim)) {
            return seed;
        }
    }
    FAIL("no seed found");
    return 0;
}

// Both players aim at paddle line `aim` for 6 s, from a power-on with the ball fast and no serve
// under way (a ball starting in a goal may still be served at once, and leave slow).
Player Rally(int aim) {
    auto loaded = BoardCircuit();
    const auto net = [&](const Simulator& sim, const char* name) {
        return sim.value(loaded->nets.at(name));
    };
    const std::uint64_t seed = FindSeed(*loaded, [&](const Simulator& sim) {
        return net(sim, "FAST") == LogicValue::One && net(sim, "SERVING") == LogicValue::Zero;
    });
    Player player(*PaddleOf(*loaded, "J3"), *PaddleOf(*loaded, "J4"), aim);
    Watch(
        *loaded, seed, 40_ms, 6'000_ms,
        [&](const TvFrame& frame, bool, Simulator&) { player.onFrame(Read(frame)); },
        PressStart(*loaded, 1_ms));  // a power-on may be game over
    return player;
}

// Every contact leaves at the speed of the quarter hit; at least one in the aimed one.
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
    CHECK(aimed >= 1);
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

namespace {

// The value of a design Net now.
int Bit(const Simulator& sim, const LoadedCircuit& loaded, const std::string& net) {
    const LogicValue value = sim.value(loaded.nets.at(net));
    REQUIRE(value != LogicValue::X);
    return value == LogicValue::One ? 1 : 0;
}

// A player's score from the counter and tens Nets (0–19; units 10–15 read as they are).
int Score(const Simulator& sim, const LoadedCircuit& loaded, char side) {
    const std::string s(1, side);
    return Bit(sim, loaded, "S" + s + "0") + 2 * Bit(sim, loaded, "S" + s + "1") +
           4 * Bit(sim, loaded, "S" + s + "2") + 8 * Bit(sim, loaded, "S" + s + "3") +
           10 * Bit(sim, loaded, std::string("TENS_") + side);
}

}  // namespace

TEST_CASE("tennis: Start clears the scores", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    auto loaded = BoardCircuit();
    Simulator sim(loaded->circuit);
    sim.powerOn(seed);
    const StepHook start = PressStart(*loaded, 1_ms);
    for (int step = 0; step < 5; ++step) {
        start(sim);
        sim.runFor(1_ms);
    }
    CHECK(Score(sim, *loaded, 'L') == 0);
    CHECK(Score(sim, *loaded, 'R') == 0);
}

namespace {

// One serve as the Nets show it (polled every 1 ms) and the pictures around it.
struct Serve {
    Time on = 0, off = 0;                                                    // SERVING rose, fell
    int leftBefore = -1, rightBefore = -1, leftAfter = -1, rightAfter = -1;  // scores
    std::vector<std::pair<Time, Picture>> pictures;  // frame end time, picture
};

// Runs from power-on with Start pressed at 1 ms; `player` plays, and stops missing once the
// first serve has ended.
Serve PlayToServe(LoadedCircuit& loaded, Player& player, Time to) {
    Serve serve;
    const StepHook start = PressStart(loaded, 1_ms);
    std::optional<bool> serving;  // unknown until the first poll: a power-on may be serving
    Watch(
        loaded, 1, 40_ms, to,
        [&](const TvFrame& frame, bool, Simulator& sim) {
            const Picture picture = Read(frame);
            player.onFrame(picture);
            serve.pictures.emplace_back(sim.now(), picture);
        },
        [&](Simulator& sim) {
            start(sim);
            if (sim.now() < 5_ms) {
                return;
            }
            const bool now = Bit(sim, loaded, "SERVING") == 1;
            if (!serving) {
                serving = now;
            }
            if (now && !*serving && serve.on == 0) {
                serve.on = sim.now();
                serve.leftAfter = Score(sim, loaded, 'L');
                serve.rightAfter = Score(sim, loaded, 'R');
            }
            if (!now && *serving && serve.on > 0 && serve.off == 0) {
                serve.off = sim.now();
                player.stopMissing();
            }
            if (serve.on == 0) {
                serve.leftBefore = Score(sim, loaded, 'L');
                serve.rightBefore = Score(sim, loaded, 'R');
            }
            serving = now;
        });
    return serve;
}

constexpr Time kFrame = 19'968_us;  // 312 lines of 64 µs

}  // namespace

TEST_CASE("tennis: a miss scores", "[machines][slow]") {
    auto loaded = BoardCircuit();
    Player player(*PaddleOf(*loaded, "J3"), *PaddleOf(*loaded, "J4"), 8, +1);
    const Serve serve = PlayToServe(*loaded, player, 6'000_ms);
    REQUIRE(serve.on > 0);
    REQUIRE(serve.off > 0);
    // Scores: the left player's point.
    CHECK(serve.leftBefore == 0);
    CHECK(serve.rightBefore == 0);
    CHECK(serve.leftAfter == 1);
    CHECK(serve.rightAfter == 0);
    // 28–32 frames (8 falls of FRAME1, every 4 frames, the first at most 4 frames in), polled to
    // the millisecond.
    const Time length = serve.off - serve.on;
    CHECK(length >= 28 * kFrame - 1_ms);
    CHECK(length <= 32 * kFrame + 1_ms);
    // No ball drawn in a frame wholly inside the serve; then it leaves the right edge, slow.
    std::vector<int> lefts;  // after the serve, each frame: the ball's column, -1 unseen
    for (const auto& [end, picture] : serve.pictures) {
        if (end - kFrame > serve.on + 1_ms && end < serve.off) {
            CHECK(!picture.ball.has_value());
        }
        if (end > serve.off + kFrame && lefts.size() < 12) {
            lefts.push_back(picture.ball && picture.ball->width == 2 ? picture.ball->left : -1);
        }
    }
    // The digits: 0 and 0 after Start, until the point; then 1 and 0. A frame with the ball on a
    // digit cell may read -1.
    int zeros = 0, ones = 0;
    for (const auto& [end, picture] : serve.pictures) {
        if (end > 3 * kFrame && end < serve.on) {
            zeros += picture.leftScore == 0 && picture.rightScore == 0 ? 1 : 0;
            CHECK((picture.leftScore == 0 || picture.leftScore == -1));
            CHECK((picture.rightScore == 0 || picture.rightScore == -1));
        }
        if (end > serve.on + kFrame) {
            ones += picture.leftScore == 1 && picture.rightScore == 0 ? 1 : 0;
            CHECK((picture.leftScore == 1 || picture.leftScore == -1 || end > serve.off));
        }
    }
    CHECK(zeros >= 10);
    CHECK(ones >= 10);
    // It leaves the right edge slow: 2 dots every other frame, so from dot 90 to dot 86 (unseen
    // on the paddle's column, 88) in 4 frames.
    REQUIRE(lefts.size() == 12);
    std::string seen;
    for (const int left : lefts) {
        seen += std::to_string(left) + " ";
    }
    INFO("after the serve: " << seen);
    const auto first = std::ranges::find_if(lefts, [](int left) { return left >= 0; });
    REQUIRE(first != lefts.end());
    CHECK(*first >= 88);  // it may come back inside a wall, unread
    const auto at90 = std::ranges::find(lefts, 90);
    const auto at86 = std::ranges::find(lefts, 86);
    REQUIRE(at90 != lefts.end());
    REQUIRE(at86 != lefts.end());
    CHECK(at86 - at90 == 4);
}

TEST_CASE("tennis: rally speeds up", "[machines][slow]") {
    auto loaded = BoardCircuit();
    Player player(*PaddleOf(*loaded, "J3"), *PaddleOf(*loaded, "J4"), 8, -1);
    const Serve serve = PlayToServe(*loaded, player, 10'000_ms);
    REQUIRE(serve.off > 0);
    // From the serve on: the ball's column each frame (-1 unseen).
    std::vector<int> lefts;
    for (const auto& [end, picture] : serve.pictures) {
        if (end > serve.off + kFrame) {
            lefts.push_back(picture.ball && picture.ball->width == 2 ? picture.ball->left : -1);
        }
    }
    // Windows of five frames seen, all moving one way: slow (4 dots) before the second turn to
    // the left, fast (8 dots) after it.
    int leftTurns = 0, direction = 0, slow = 0, fast = 0;
    for (std::size_t k = 4; k < lefts.size(); ++k) {
        const int a = lefts[k - 4], b = lefts[k];
        bool seen = true;
        for (std::size_t j = k - 4; j <= k; ++j) {
            seen = seen && lefts[j] >= 0;
        }
        if (lefts[k] >= 0 && lefts[k - 1] >= 0 && lefts[k] != lefts[k - 1]) {
            const int now = lefts[k] > lefts[k - 1] ? 1 : -1;
            if (direction > 0 && now < 0) {
                ++leftTurns;
            }
            direction = now;
        }
        if (!seen) {
            continue;
        }
        bool oneWay = true;
        for (std::size_t j = k - 3; j <= k; ++j) {
            oneWay = oneWay && (lefts[j] - lefts[j - 1]) * (b - a) >= 0;
        }
        if (!oneWay || a == b) {
            continue;
        }
        INFO("frame " << k << ", left turns " << leftTurns << ", moved " << b - a);
        if (leftTurns < 2) {
            CHECK(std::abs(b - a) == 4);
            ++slow;
        } else {
            CHECK(std::abs(b - a) == 8);
            ++fast;
        }
    }
    CHECK(leftTurns >= 2);
    CHECK(slow >= 10);
    CHECK(fast >= 3);
}

TEST_CASE("tennis: score digits", "[machines]") {
    auto loaded = BoardCircuit();
    const auto bit = [&](const Simulator& sim, const char* name) {
        return sim.value(loaded->nets.at(name)) == LogicValue::One ? 1 : 0;
    };
    // A power-on with the left score at 10 and the right one a digit.
    const std::uint64_t seed = FindSeed(*loaded, [&](const Simulator& sim) {
        return bit(sim, "TENS_L") == 1 &&
               bit(sim, "SL0") + bit(sim, "SL1") + bit(sim, "SL2") + bit(sim, "SL3") == 0 &&
               bit(sim, "TENS_R") == 0 &&
               !(bit(sim, "SR3") && (bit(sim, "SR1") || bit(sim, "SR2"))) &&
               bit(sim, "SERVING") == 0;
    });
    int right = -2;
    {
        Simulator sim(loaded->circuit);
        sim.powerOn(seed);
        sim.runFor(1_us);
        right = bit(sim, "SR0") + 2 * bit(sim, "SR1") + 4 * bit(sim, "SR2") + 8 * bit(sim, "SR3");
    }
    int read = 0;
    Watch(*loaded, seed, 40_ms, 700_ms, [&](const TvFrame& frame, bool, Simulator&) {
        const Picture picture = Read(frame);
        if (picture.leftScore >= 0 && picture.rightScore >= 0) {
            CHECK(picture.leftScore == 10);
            CHECK(picture.rightScore == right);
            ++read;
        }
    });
    CHECK(read >= 3);
}

TEST_CASE("tennis: game over", "[machines][slow]") {
    auto loaded = BoardCircuit();
    const auto bit = [&](const Simulator& sim, const char* name) {
        return sim.value(loaded->nets.at(name)) == LogicValue::One ? 1 : 0;
    };
    // A power-on with the game on, the left score at 10, the right one a digit, no serve.
    const std::uint64_t seed = FindSeed(*loaded, [&](const Simulator& sim) {
        return bit(sim, "GAME_OVER") == 0 && bit(sim, "TENS_L") == 1 &&
               bit(sim, "SL0") + bit(sim, "SL1") + bit(sim, "SL2") + bit(sim, "SL3") == 0 &&
               bit(sim, "TENS_R") == 0 &&
               !(bit(sim, "SR3") && (bit(sim, "SR1") || bit(sim, "SR2"))) &&
               bit(sim, "SERVING") == 0;
    });
    Player player(*PaddleOf(*loaded, "J3"), *PaddleOf(*loaded, "J4"), 8, +1);
    Switch* button = SwitchOf(*loaded, "SW1");
    REQUIRE(button != nullptr);
    Time over = 0;  // GAME_OVER rose
    int right = -1;
    std::vector<std::pair<Time, Picture>> pictures;
    Watch(
        *loaded, seed, 40_ms, 7'000_ms,
        [&](const TvFrame& frame, bool, Simulator& sim) {
            const Picture picture = Read(frame);
            player.onFrame(picture);
            pictures.emplace_back(sim.now(), picture);
        },
        [&](Simulator& sim) {
            if (over == 0 && Bit(sim, *loaded, "GAME_OVER") == 1) {
                over = sim.now();
                CHECK(Score(sim, *loaded, 'L') == 11);
                right = Score(sim, *loaded, 'R');  // a ball starting in a goal may have scored
                CHECK(right < 10);
            }
            // Start 3 s into the game over.
            if (over > 0 && (sim.now() == over + 3'000_ms || sim.now() == over + 3'002_ms)) {
                button->setPressed(sim.now() == over + 3'000_ms);
                player.stopMissing();
                sim.wake(button->id());
            }
        });
    REQUIRE(over > 0);
    // During the game over: no paddles, the scores 11 and the right one, the ball turning at an
    // edge it is seen at (no miss); after Start: 0 and 0, the paddles back.
    int during = 0, after = 0;
    bool edge = false;
    for (const auto& [end, picture] : pictures) {
        if (end > over + 2 * kFrame && end < over + 3'000_ms) {
            // A paddle is 16 lines; the ball crossing a paddle's column is 8.
            CHECK((!picture.left || picture.left->height != 16));
            CHECK((!picture.right || picture.right->height != 16));
            if (picture.leftScore >= 0 && picture.rightScore >= 0) {
                CHECK(picture.leftScore == 11);
                CHECK(picture.rightScore == right);
                ++during;
            }
            edge = edge || (picture.ball && (picture.ball->left == 2 || picture.ball->left == 92));
        }
        if (end > over + 3'000_ms + 2 * kFrame && end < over + 3'300_ms) {
            CHECK(picture.left.has_value());
            CHECK(picture.right.has_value());
            if (picture.leftScore >= 0 && picture.rightScore >= 0) {
                CHECK(picture.leftScore == 0);
                CHECK(picture.rightScore == 0);
                ++after;
            }
        }
    }
    CHECK(during >= 5);
    CHECK(edge);
    CHECK(after >= 3);
}

// Puzzles (TENNIS.md, Puzzles): each run from power-on with Start pressed, faulty then repaired.

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

struct Shot {
    TvFrame frame;
    Picture picture;
    bool locked;
};

struct Run {
    std::vector<Shot> shots;         // frames that started at or after `from`
    std::vector<std::string> lamps;  // one per probe
    int count(const std::function<bool(const Shot&)>& wanted) const {
        return static_cast<int>(std::ranges::count_if(shots, wanted));
    }
};

// From power-on (seed 1) with the knobs set and Start pressed at 1 ms (a power-on may be game
// over).
Run Play(LoadedCircuit& loaded, double left, double right, Time from, Time to,
         const std::vector<std::string>& probes = {}) {
    Paddle* j3 = PaddleOf(loaded, "J3");
    Paddle* j4 = PaddleOf(loaded, "J4");
    REQUIRE(j3 != nullptr);
    REQUIRE(j4 != nullptr);
    Run run;
    run.lamps = Watch(
        loaded, 1, from, to,
        [&](const TvFrame& frame, bool locked, Simulator&) {
            run.shots.push_back({frame, Read(frame), locked});
        },
        [&, start = PressStart(loaded, 1_ms)](Simulator& sim) {
            if (sim.now() == 0) {
                j3->setKnob(left);
                j4->setKnob(right);
            }
            start(sim);
        },
        probes);
    return run;
}

bool Pulses(const std::string& lamps) { return lamps.find("PULSE") != std::string::npos; }

bool PaddleIs(const std::optional<PaddleSeen>& paddle, int top, int height) {
    return paddle && paddle->top == top && paddle->height == height;
}

}  // namespace

TEST_CASE("tennis: Puzzles have a title and a brief", "[machines]") {
    for (const char* name :
         {"mirror_paddle", "shy_paddles", "paddle_fence", "long_paddle", "invisible_ball",
          "double_scores", "folded_digits", "rolling_picture", "short_frames"}) {
        INFO(name);
        const auto puzzle = ReadPuzzleFile(test::PuzzlePath(name));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "tennis");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("tennis: mirror_paddle", "[machines]") {
    // Left knob at its left stop (top 224), right knob at its right stop (top 16).
    Machine machine = LoadPuzzle("mirror_paddle");
    {
        auto loaded = Build(machine);
        const Run run = Play(*loaded, 0.0, 1.0, 550_ms, 800_ms);
        CHECK(run.count([](const Shot& s) { return PaddleIs(s.picture.right, 224, 16); }) >= 5);
        CHECK(run.count([](const Shot& s) {
            return PaddleIs(s.picture.right, 16, 16);  // where the right knob would put it
        }) == 0);
    }
    REQUIRE(machine.swap("U31"));
    auto loaded = Build(machine);
    const Run run = Play(*loaded, 0.0, 1.0, 550_ms, 800_ms);
    CHECK(run.count([](const Shot& s) { return PaddleIs(s.picture.right, 16, 16); }) >= 5);
}

TEST_CASE("tennis: shy_paddles", "[machines]") {
    // Left paddle lines 120–135, right 128–143: they share lines 128–135.
    Machine machine = LoadPuzzle("shy_paddles");
    const double right = KnobForTop(128);
    {
        auto loaded = Build(machine);
        CHECK(*PinNet(*loaded, "U31", "3A") == *PinNet(*loaded, "U31", "3B"));
        const Run run = Play(*loaded, 0.5, right, 550_ms, 800_ms);
        CHECK(run.count([](const Shot& s) {
            return PaddleIs(s.picture.left, 128, 8) && PaddleIs(s.picture.right, 128, 8);
        }) >= 5);
        CHECK(run.count([](const Shot& s) { return PaddleIs(s.picture.left, 120, 16); }) == 0);
    }
    REQUIRE(machine.removeBridge({"U31", "10"}, {"U31", "11"}));
    auto loaded = Build(machine);
    const Run run = Play(*loaded, 0.5, right, 550_ms, 800_ms);
    CHECK(run.count([](const Shot& s) {
        return PaddleIs(s.picture.left, 120, 16) && PaddleIs(s.picture.right, 128, 16);
    }) >= 5);
}

TEST_CASE("tennis: paddle_fence", "[machines]") {
    Machine machine = LoadPuzzle("paddle_fence");
    // Copies of the left paddle (lines 120–135) at dots 14, 22 and 30.
    const auto fence = [](const Shot& s) {
        if (!s.picture.walls) {
            return false;
        }
        for (const int dot : {14, 22, 30}) {
            if (!PaddleIs(ReadPaddle(s.frame, s.picture.lineZeroRow, dot), 120, 16)) {
                return false;
            }
        }
        return true;
    };
    const Point pin4 = *machine.padPosition("U33", "4");
    {
        auto loaded = Build(machine);
        CHECK(machine.probeNet(*loaded, Layer::Back, pin4) == PinNet(*loaded, "U47", "4Y"));
        CHECK(*PinNet(*loaded, "U33", "1C") != *PinNet(*loaded, "U47", "4Y"));  // the leg
        const Run run = Play(*loaded, 0.5, 0.0, 550_ms, 800_ms);
        CHECK(run.count(fence) >= 5);
        CHECK(run.count([](const Shot& s) { return PaddleIs(s.picture.left, 120, 16); }) >= 5);
    }
    REQUIRE(machine.resolder({"U33", "4"}));
    auto loaded = Build(machine);
    const Run run = Play(*loaded, 0.5, 0.0, 550_ms, 800_ms);
    CHECK(run.count(fence) == 0);
    CHECK(run.count([](const Shot& s) { return PaddleIs(s.picture.left, 120, 16); }) >= 5);
}

TEST_CASE("tennis: long_paddle", "[machines]") {
    // Left knob at its right stop: the paddle's top on line 16.
    Machine machine = LoadPuzzle("long_paddle");
    {
        auto loaded = Build(machine);
        const Run run = Play(*loaded, 1.0, 0.0, 550_ms, 800_ms, {"U32.1Y", "U28.CKA"});
        // Down to the court's last line (and on, unseen, to the next trigger).
        CHECK(run.count([](const Shot& s) { return PaddleIs(s.picture.left, 16, 224); }) >= 5);
        CHECK(Pulses(run.lamps[0]));
        CHECK(run.lamps[1] == "-");
    }
    REQUIRE(machine.mend(LongPaddleCrack().layer, LongPaddleCrack().at));
    auto loaded = Build(machine);
    const Run run = Play(*loaded, 1.0, 0.0, 550_ms, 800_ms);
    CHECK(run.count([](const Shot& s) { return PaddleIs(s.picture.left, 16, 16); }) >= 5);
}

TEST_CASE("tennis: invisible_ball", "[machines][slow]") {
    // The paddles at opposite corners: the unseen ball is missed again and again.
    Machine machine = LoadPuzzle("invisible_ball");
    {
        auto loaded = Build(machine);
        const Run run = Play(*loaded, 0.0, 1.0, 550_ms, 8'000_ms, {"U43.3B", "U38.3B"});
        CHECK(run.count([](const Shot& s) { return s.picture.walls; }) >= 300);
        CHECK(run.count([](const Shot& s) { return s.picture.ball.has_value(); }) == 0);
        int left = 0, right = 0;
        for (const Shot& shot : run.shots) {
            left = std::max(left, shot.picture.leftScore);
            right = std::max(right, shot.picture.rightScore);
        }
        CHECK(left + right >= 2);
        CHECK(run.lamps[0] != "-");  // SERVING, driven at U43
        CHECK(run.lamps[1] == "-");
    }
    REQUIRE(machine.mend(InvisibleBallCrack().layer, InvisibleBallCrack().at));
    auto loaded = Build(machine);
    const Run run = Play(*loaded, 0.0, 1.0, 550_ms, 1'500_ms);
    CHECK(run.count([](const Shot& s) { return s.picture.ball.has_value(); }) >= 10);
}

TEST_CASE("tennis: double_scores", "[machines]") {
    // Both paddles low, away from the digits.
    Machine machine = LoadPuzzle("double_scores");
    const auto copied = [](const Shot& s) {
        const int z = s.picture.lineZeroRow;
        const int left = ReadDigit(s.frame, z, 8), right = ReadDigit(s.frame, z, 88);
        return s.picture.walls && left >= 0 && left == ReadDigit(s.frame, z, 24) && right >= 0 &&
               right == ReadDigit(s.frame, z, 72);
    };
    const auto dark = [](const Shot& s) {
        if (!s.picture.walls) {
            return false;
        }
        for (int line = 32; line < 64; ++line) {
            for (const int dot : {8, 9, 10, 11, 88, 89, 90, 91}) {
                if (LitCell(s.frame, s.picture.lineZeroRow + line, dot)) {
                    return false;
                }
            }
        }
        return true;
    };
    {
        auto loaded = Build(machine);
        const Run run = Play(*loaded, 0.0, 0.0, 550_ms, 800_ms);
        CHECK(run.count(copied) >= 5);
        CHECK(run.count(dark) == 0);
    }
    REQUIRE(machine.swap("U10"));
    auto loaded = Build(machine);
    const Run run = Play(*loaded, 0.0, 0.0, 550_ms, 800_ms);
    CHECK(run.count(copied) == 0);
    CHECK(run.count(dark) >= 5);
}

TEST_CASE("tennis: folded_digits", "[machines]") {
    Machine machine = LoadPuzzle("folded_digits");
    {
        auto loaded = Build(machine);
        const Run run = Play(*loaded, 0.0, 0.0, 550_ms, 800_ms);
        // 0 and 0 drawn as their bottom halves twice: no digit, and never a top bar.
        CHECK(run.count([](const Shot& s) {
            return s.picture.walls && s.picture.leftScore == -1 && s.picture.rightScore == -1;
        }) >= 5);
        CHECK(run.count([](const Shot& s) {
            const int z = s.picture.lineZeroRow;
            return s.picture.walls &&
                   (LitCell(s.frame, z + 33, 25) || LitCell(s.frame, z + 33, 73));
        }) == 0);
    }
    REQUIRE(machine.swap("U54"));
    auto loaded = Build(machine);
    const Run run = Play(*loaded, 0.0, 0.0, 550_ms, 800_ms);
    CHECK(run.count([](const Shot& s) {
        return s.picture.leftScore >= 0 && s.picture.rightScore >= 0;
    }) >= 5);
}

TEST_CASE("tennis: rolling_picture", "[machines]") {
    Machine machine = LoadPuzzle("rolling_picture");
    const Point pin2 = *machine.padPosition("U10", "2");
    {
        auto loaded = Build(machine);
        CHECK(machine.probeNet(*loaded, Layer::Back, pin2) == PinNet(*loaded, "U38", "4Y"));
        CHECK(*PinNet(*loaded, "U10", "1B") != *PinNet(*loaded, "U38", "4Y"));  // the leg
        // TICK_N (line 308) pulses at U8 and triggers the controllers, apart from the sync
        // (P6n): the paddles work; the leg floats.
        const Run run = Play(*loaded, 0.5, 0.5, 550_ms, 1'200_ms, {"U8.1Y", "J3.TRIG_N", "U10.1B"});
        CHECK(run.shots.size() >= 25);
        CHECK(run.count([](const Shot& s) { return s.locked; }) == 0);
        CHECK(Pulses(run.lamps[0]));
        CHECK(Pulses(run.lamps[1]));
        CHECK(run.lamps[2] == "-");
    }
    REQUIRE(machine.resolder({"U10", "2"}));
    auto loaded = Build(machine);
    const Run run = Play(*loaded, 0.5, 0.5, 550_ms, 1'200_ms);
    CHECK(run.count([](const Shot& s) { return !s.locked || !s.picture.walls; }) == 0);
}

TEST_CASE("tennis: short_frames", "[machines]") {
    Machine machine = LoadPuzzle("short_frames");
    {
        auto loaded = Build(machine);
        CHECK(*PinNet(*loaded, "U7", "D") != *PinNet(*loaded, "U3", "QC"));  // the leg
        const Run run = Play(*loaded, 0.5, 0.5, 550_ms, 1'200_ms, {"U7.D", "U3.QC", "U8.1Y"});
        CHECK(run.shots.size() >= 25);
        CHECK(run.lamps[0] == "-");
        CHECK(Pulses(run.lamps[1]));
        CHECK(run.lamps[2].find("LOW") == std::string::npos);  // no TICK_N
        // The sync (lines 272-287) still comes, so the picture holds (P6n); no paddle is drawn,
        // and without MOVE the ball stays on its column.
        CHECK(run.count([](const Shot& s) { return !s.locked || !s.picture.walls; }) == 0);
        CHECK(run.count([](const Shot& s) { return s.picture.left || s.picture.right; }) == 0);
        std::optional<int> column;
        for (const Shot& shot : run.shots) {
            if (shot.picture.ball) {
                CHECK(shot.picture.ball->left == column.value_or(shot.picture.ball->left));
                column = shot.picture.ball->left;
            }
        }
    }
    REQUIRE(machine.resolder({"U7", "4"}));
    auto loaded = Build(machine);
    const Run run = Play(*loaded, 0.5, 0.5, 550_ms, 1'200_ms);
    CHECK(run.count([](const Shot& s) { return !s.locked || !s.picture.walls; }) == 0);
    CHECK(run.count([](const Shot& s) { return PaddleIs(s.picture.left, 120, 16); }) >= 5);
}
