#include "view/digit_shape.hpp"

#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace bw;
using Catch::Approx;

namespace {

constexpr double kMm = 1'000'000;

// The LTS footprint's courtyard (local mm, KiCad y down) and pad 1 at the origin, turned by
// `quarter` quarter turns about the origin and moved to (100, 50) mm.
struct Placed {
    std::vector<Point> courtyard;
    Point pad1;
};

Point Turn(double x, double y, int quarter) {
    for (int k = 0; k < quarter; ++k) {
        const double t = x;
        x = y;
        y = -t;
    }
    return {std::llround((100 + x) * kMm), std::llround((50 + y) * kMm)};
}

Placed Lts(int quarter) {
    return {{Turn(-2.16, -1.47, quarter), Turn(17.4, -1.47, quarter), Turn(17.4, 11.63, quarter),
             Turn(-2.16, 11.63, quarter)},
            Turn(0, 0, quarter)};
}

// Position of p along the frame's axes from its centre, mm.
double Along(const DigitFrame& f, Point p, bool up) {
    const double x = static_cast<double>(p.x) - f.centreX;
    const double y = static_cast<double>(p.y) - f.centreY;
    return (up ? x * f.upX + y * f.upY : x * f.rightX + y * f.rightY) / kMm;
}

double Mean(const DigitFrame& f, LampShape shape, bool up) {
    double sum = 0;
    const auto outline = LampOutline(f, shape);
    for (const Point& p : outline) {
        sum += Along(f, p, up);
    }
    return sum / static_cast<double>(outline.size());
}

}  // namespace

TEST_CASE("digit shape: the digit stands on its pad 1 row at any rotation", "[view]") {
    for (int quarter = 0; quarter < 4; ++quarter) {
        INFO(quarter);
        const Placed lts = Lts(quarter);
        const auto frame = DigitFrameOf(lts.courtyard, lts.pad1);
        REQUIRE(frame.has_value());
        CHECK(frame->length / kMm == Approx(19.56));
        CHECK(frame->breadth / kMm == Approx(13.1));
        CHECK(Along(*frame, lts.pad1, true) < 0);  // pad 1 below the centre
        // right is up turned a quarter clockwise on the front view
        CHECK(frame->rightX == Approx(-frame->upY));
        CHECK(frame->rightY == Approx(frame->upX));

        CHECK(Mean(*frame, LampShape::SegmentA, true) > Mean(*frame, LampShape::SegmentG, true));
        CHECK(Mean(*frame, LampShape::SegmentG, true) > Mean(*frame, LampShape::SegmentD, true));
        CHECK(Mean(*frame, LampShape::SegmentB, false) > Mean(*frame, LampShape::SegmentF, false));
        CHECK(Mean(*frame, LampShape::SegmentC, false) > Mean(*frame, LampShape::SegmentE, false));
        CHECK(Mean(*frame, LampShape::DecimalPoint, false) >
              Mean(*frame, LampShape::SegmentC, false));
        for (int s = 1; s <= 8; ++s) {
            for (const Point& p : LampOutline(*frame, static_cast<LampShape>(s))) {
                CHECK(std::abs(Along(*frame, p, true)) < 19.56 / 2);
                CHECK(std::abs(Along(*frame, p, false)) < 13.1 / 2);
            }
        }
    }
}

TEST_CASE("digit shape: upright on the Board when not rotated", "[view]") {
    // Pads 1..5 at the bottom: pad 1 below the centre on the screen (y down).
    const std::vector<Point> courtyard{
        {0, 0}, {13'000'000, 0}, {13'000'000, 19'000'000}, {0, 19'000'000}};
    const auto frame = DigitFrameOf(courtyard, {1'000'000, 18'000'000});
    REQUIRE(frame.has_value());
    CHECK(frame->upX == Approx(0).margin(1e-12));
    CHECK(frame->upY == Approx(-1));
    CHECK(frame->rightX == Approx(1));
}

TEST_CASE("digit shape: needs four corners", "[view]") {
    const std::vector<Point> three{{0, 0}, {1, 0}, {1, 1}};
    CHECK_FALSE(DigitFrameOf(three, {0, 0}).has_value());
}
