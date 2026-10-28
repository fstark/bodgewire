#include "view/viewport.hpp"

#include <cmath>
#include <cstdlib>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

constexpr std::int64_t kMm = 1'000'000;
constexpr Bounds kBoard{{10 * kMm, 20 * kMm}, {110 * kMm, 70 * kMm}};  // 100 × 50 mm
constexpr ScreenPoint kPanel{800, 600};

bool Near(Point a, Point b, std::int64_t tolerance) {
    return std::abs(a.x - b.x) <= tolerance && std::abs(a.y - b.y) <= tolerance;
}

}  // namespace

TEST_CASE("view: viewport fit centres the Board with the margin", "[view]") {
    Viewport v;
    v.fit(kBoard, kPanel);
    CHECK(v.pixelsPerMm() == Catch::Approx(Viewport::kFitMargin * 8));
    const ScreenPoint min = v.toScreen(kBoard.min);
    const ScreenPoint max = v.toScreen(kBoard.max);
    CHECK((min.x + max.x) / 2 == Catch::Approx(400));
    CHECK((min.y + max.y) / 2 == Catch::Approx(300));
    CHECK(v.fitted());
}

TEST_CASE("view: viewport round trip", "[view]") {
    Viewport v;
    v.fit(kBoard, kPanel);
    v.zoomAt({123, 456}, 7);
    for (const Point p : {Point{10 * kMm, 20 * kMm}, Point{55'555'555, 33'333'333}}) {
        CHECK(Near(v.toBoard(v.toScreen(p)), p, 1));
    }
}

TEST_CASE("view: viewport zoom keeps the point under the cursor", "[view]") {
    Viewport v;
    v.fit(kBoard, kPanel);
    const ScreenPoint cursor{200, 150};
    const Point before = v.toBoard(cursor);
    v.zoomAt(cursor, 3);
    CHECK_FALSE(v.fitted());
    const ScreenPoint after = v.toScreen(before);
    CHECK(after.x == Catch::Approx(cursor.x).margin(0.01));
    CHECK(after.y == Catch::Approx(cursor.y).margin(0.01));
}

TEST_CASE("view: viewport zoom clamps", "[view]") {
    Viewport v;
    v.fit(kBoard, kPanel);
    const double fitted = v.pixelsPerMm();
    v.zoomAt({400, 300}, -5);
    CHECK(v.pixelsPerMm() == Catch::Approx(fitted));
    CHECK(v.fitted());
    v.zoomAt({400, 300}, 100);
    CHECK(v.pixelsPerMm() == Catch::Approx(Viewport::kMaxPixelsPerMm));
}

TEST_CASE("view: viewport resize", "[view]") {
    Viewport v;
    v.fit(kBoard, kPanel);
    v.resize({400, 600});
    CHECK(v.pixelsPerMm() == Catch::Approx(Viewport::kFitMargin * 4));
    CHECK(v.fitted());

    v.zoomAt({200, 300}, 2);
    v.pan({30, -20});
    CHECK_FALSE(v.fitted());
    const Point centre = v.toBoard({200, 300});
    const double scale = v.pixelsPerMm();
    v.resize({1000, 500});
    CHECK(v.pixelsPerMm() == Catch::Approx(scale));
    CHECK(Near(v.toBoard({500, 250}), centre, 1));
}

TEST_CASE("view: viewport frame", "[view]") {
    Viewport v;
    v.fit(kBoard, kPanel);
    const Bounds box{{50 * kMm, 40 * kMm}, {60 * kMm, 45 * kMm}};  // 10 × 5 mm
    v.frame(box, 0.5, 0);
    CHECK(v.pixelsPerMm() == Catch::Approx(40));  // 0.5 × min(800 / 10, 600 / 5)
    CHECK_FALSE(v.fitted());
    const ScreenPoint centre = v.toScreen({55 * kMm, 42'500'000});
    CHECK(centre.x == Catch::Approx(400));
    CHECK(centre.y == Catch::Approx(300));

    v.frame(box, 0.5, 70);
    CHECK(v.pixelsPerMm() == Catch::Approx(70));
    v.frame(box, 0.5, 500);
    CHECK(v.pixelsPerMm() == Catch::Approx(Viewport::kMaxPixelsPerMm));
    v.frame(kBoard, 0.1, 0);
    CHECK(v.pixelsPerMm() == Catch::Approx(Viewport::kFitMargin * 8));

    v.frame({{30 * kMm, 30 * kMm}, {30 * kMm, 30 * kMm}}, 0.5, 20);
    CHECK(v.pixelsPerMm() == Catch::Approx(20));
    const ScreenPoint point = v.toScreen({30 * kMm, 30 * kMm});
    CHECK(point.x == Catch::Approx(400));
    CHECK(point.y == Catch::Approx(300));
}
