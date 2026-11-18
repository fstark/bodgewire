#include "view/pencil_marks.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {
constexpr std::int64_t kReach = 1'000'000;  // nm
}

TEST_CASE("pencil marks: a click on bare Board puts a dot, clicks on it go check, cross, none",
          "[view]") {
    PencilMarks marks;
    marks.click({5'000'000, 5'000'000}, std::nullopt, kReach);
    REQUIRE(marks.marks().size() == 1);
    CHECK(marks.marks()[0].shape == MarkShape::Dot);
    CHECK(marks.marks()[0].at == Point{5'000'000, 5'000'000});
    CHECK_FALSE(marks.marks()[0].body);
    marks.click({5'400'000, 5'300'000}, std::nullopt, kReach);  // 0.5 mm off: on it
    REQUIRE(marks.marks().size() == 1);
    CHECK(marks.marks()[0].shape == MarkShape::Check);
    CHECK(marks.marks()[0].at == Point{5'000'000, 5'000'000});  // not moved
    marks.click({5'000'000, 5'000'000}, std::nullopt, kReach);
    CHECK(marks.marks()[0].shape == MarkShape::Cross);
    marks.click({5'000'000, 5'000'000}, std::nullopt, kReach);
    CHECK(marks.marks().empty());
}

TEST_CASE("pencil marks: a click beyond the reach puts a second mark, on its body", "[view]") {
    PencilMarks marks;
    marks.click({0, 0}, std::nullopt, kReach);
    marks.click({1'000'001, 0}, 3, kReach);
    REQUIRE(marks.marks().size() == 2);
    CHECK(marks.marks()[0].shape == MarkShape::Dot);
    CHECK(marks.marks()[1].shape == MarkShape::Dot);
    CHECK(marks.marks()[1].body == 3);
}

TEST_CASE("pencil marks: of two in reach the nearest changes, the later on a tie", "[view]") {
    PencilMarks marks;
    marks.add({{0, 0}, MarkShape::Dot, std::nullopt});
    marks.add({{1'200'000, 0}, MarkShape::Dot, std::nullopt});
    marks.click({800'000, 0}, std::nullopt, kReach);  // 0.8 mm and 0.4 mm away
    CHECK(marks.marks()[0].shape == MarkShape::Dot);
    CHECK(marks.marks()[1].shape == MarkShape::Check);
    marks.add({{0, 0}, MarkShape::Dot, std::nullopt});  // on top of the first
    marks.click({0, 0}, std::nullopt, kReach);
    CHECK(marks.marks()[0].shape == MarkShape::Dot);
    CHECK(marks.marks()[2].shape == MarkShape::Check);
}

TEST_CASE("pencil marks: a clear removes the nearest, or reports none in reach", "[view]") {
    PencilMarks marks;
    marks.add({{0, 0}, MarkShape::Cross, std::nullopt});
    marks.add({{3'000'000, 0}, MarkShape::Check, std::nullopt});
    CHECK_FALSE(marks.clear({1'500'000, 0}, kReach));
    CHECK(marks.marks().size() == 2);
    CHECK(marks.clear({2'500'000, 0}, kReach));
    REQUIRE(marks.marks().size() == 1);
    CHECK(marks.marks()[0].shape == MarkShape::Cross);
}

TEST_CASE("pencil marks: a swap removes the marks on that body only", "[view]") {
    PencilMarks marks;
    marks.add({{0, 0}, MarkShape::Dot, 2});
    marks.add({{1'000'000, 0}, MarkShape::Check, std::nullopt});
    marks.add({{2'000'000, 0}, MarkShape::Cross, 5});
    marks.add({{3'000'000, 0}, MarkShape::Dot, 2});
    marks.swapped(2);
    REQUIRE(marks.marks().size() == 2);
    CHECK_FALSE(marks.marks()[0].body);
    CHECK(marks.marks()[1].body == 5);
}
