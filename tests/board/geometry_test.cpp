#include "board/geometry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace bw;
using Catch::Matchers::WithinAbs;

TEST_CASE("geometry: the nearest point of a segment", "[board]") {
    const Point a{0, 0}, b{1'000, 0};
    CHECK(NearestT({250, 400}, a, b) == 0.25);
    CHECK(NearestT({-50, 10}, a, b) == 0.0);  // before a
    CHECK(NearestT({1'500, 0}, a, b) == 1.0);
    CHECK(NearestT({7, 7}, a, a) == 0.0);  // a point, not a segment
    CHECK(NearestOnSegment({250, 400}, a, b) == Point{250, 0});
    CHECK(NearestOnSegment({1'500, 9}, a, b) == b);
}

TEST_CASE("geometry: distance to a segment", "[board]") {
    const Point a{0, 0}, b{0, 1'000};
    CHECK(SegmentDistance({300, 500}, a, b) == 300.0);
    CHECK(SegmentDistance({300, 1'400}, a, b) == 500.0);  // past b: to b
    CHECK(SegmentDistance({3, 4}, a, a) == 5.0);
    CHECK(Distance({0, 0}, {-3, 4}) == 5.0);
}

TEST_CASE("geometry: offsets turn the KiCad way", "[board]") {
    const Point origin{10'000, 20'000};
    CHECK(RotatedOffset(origin, 1'000, 0, 0) == Point{11'000, 20'000});
    CHECK(RotatedOffset(origin, 1'000, 0, 90) == Point{10'000, 19'000});  // up: y grows down
    CHECK(RotatedOffset(origin, 0, 1'000, 90) == Point{11'000, 20'000});
    CHECK(RotatedOffset(origin, 1'000, 0, 180) == Point{9'000, 20'000});
    const Point turned = RotatedOffset({0, 0}, 1'000, 0, 45);
    CHECK_THAT(Distance({0, 0}, turned), WithinAbs(1'000, 1));
}
