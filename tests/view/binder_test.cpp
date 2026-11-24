#include "view/binder.hpp"

#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("binder: comes out at the User Manual, even with a chip selected", "[binder]") {
    Binder binder;
    binder.frame(false, 3);
    binder.frame(true, 3);
    CHECK(binder.divider() == Divider::User);
    binder.frame(true, 3);
    CHECK(binder.divider() == Divider::User);  // the same selection does not turn it
}

TEST_CASE("binder: a selection while out turns to Datasheets from any divider", "[binder]") {
    for (const Divider from : {Divider::User, Divider::Service, Divider::Repairs}) {
        Binder binder;
        binder.frame(true, std::nullopt);
        binder.turn(from);
        binder.frame(true, std::nullopt);
        CHECK(binder.divider() == from);
        binder.frame(true, 2);
        CHECK(binder.divider() == Divider::Datasheets);
    }
    Binder binder;
    binder.frame(true, 2);
    binder.turn(Divider::Service);
    binder.frame(true, 5);  // another chip
    CHECK(binder.divider() == Divider::Datasheets);
}

TEST_CASE("binder: a deselection does not turn it", "[binder]") {
    Binder binder;
    binder.frame(true, 2);
    binder.turn(Divider::Service);
    binder.frame(true, std::nullopt);
    CHECK(binder.divider() == Divider::Service);
}

TEST_CASE("binder: a selection made while away does not turn it when it comes out", "[binder]") {
    Binder binder;
    binder.frame(true, std::nullopt);
    binder.turn(Divider::Service);
    binder.frame(false, std::nullopt);
    binder.frame(false, 4);
    binder.frame(true, 4);
    CHECK(binder.divider() == Divider::User);
}

TEST_CASE("binder: show picks the divider it comes out at, once", "[binder]") {
    Binder binder;
    binder.show(Divider::Datasheets);
    binder.frame(true, 1);  // --manual datasheet --probe U1.1: the start-up selection seen
    CHECK(binder.divider() == Divider::Datasheets);
    binder.frame(true, 1);
    CHECK(binder.divider() == Divider::Datasheets);
    binder.turn(Divider::Repairs);
    binder.frame(false, 1);
    binder.frame(true, 1);
    CHECK(binder.divider() == Divider::User);

    Binder user;
    user.show(Divider::User);
    user.frame(true, 1);  // --manual user --probe U1.1
    user.frame(true, 1);
    CHECK(user.divider() == Divider::User);
}

TEST_CASE("binder: a Part opened from the index, closed by a selection or coming out", "[binder]") {
    Binder binder;
    binder.frame(true, std::nullopt);
    binder.turn(Divider::Datasheets);
    binder.openPart("74LS90");
    binder.frame(true, std::nullopt);
    REQUIRE(binder.part());
    CHECK(*binder.part() == "74LS90");
    binder.closePart();
    CHECK_FALSE(binder.part());

    binder.openPart("74LS47");
    binder.frame(true, 6);
    CHECK_FALSE(binder.part());

    binder.openPart("74LS47");
    binder.frame(false, 6);
    binder.frame(true, 6);
    CHECK_FALSE(binder.part());
}

TEST_CASE("binder: a memory chip's window opens Contents, out or away", "[binder]") {
    Binder away;
    CHECK_FALSE(away.contents().has_value());
    away.frame(false, std::nullopt);
    away.openContents(4);
    away.frame(true, std::nullopt);  // the click brings it out
    CHECK(away.divider() == Divider::Contents);
    CHECK(away.contents() == std::size_t{4});

    Binder out;
    out.frame(true, std::nullopt);
    out.turn(Divider::Service);
    out.openContents(7);
    CHECK(out.divider() == Divider::Contents);
    out.frame(true, std::nullopt);
    CHECK(out.divider() == Divider::Contents);
    out.frame(true, 2);  // a chip selected: its datasheet
    CHECK(out.divider() == Divider::Datasheets);
    CHECK(out.contents() == std::size_t{7});  // the tab stays
    out.frame(false, 2);
    out.frame(true, 2);  // out again from the shelf: the User Manual, not Contents
    CHECK(out.divider() == Divider::User);
}

TEST_CASE("binder: tabs a step apart", "[binder]") {
    // Four tabs of 44 px from 30 px down a binder 500 px tall: 50 px apart, as drawn since P6e.
    CHECK(TabTops(4, 30, 486, 44, 50) == std::vector<float>{30, 80, 130, 180});
    CHECK(TabTops(6, 30, 486, 44, 50) == std::vector<float>{30, 80, 130, 180, 230, 280});
    // The first size's height (500 px for 600 wide) takes six: the last ends at 324.
    CHECK(TabTops(6, 30, 500 - 14, 44, 50).back() + 44 <= 500 - 14);
}

TEST_CASE("binder: six tabs in the smallest binder", "[binder]") {
    // 292 px, reached by a drag (500 wide, 0.7 of its proportions): the six close up, evenly, and
    // the last ends inside the cover.
    const float bottom = 292 - 14;
    const std::vector<float> tops = TabTops(6, 30, bottom, 44, 50);
    REQUIRE(tops.size() == 6);
    CHECK(tops.front() == 30);
    CHECK(tops.back() + 44 <= bottom + 0.01f);
    for (std::size_t i = 1; i < tops.size(); ++i) {
        const float step = tops[i] - tops[i - 1];
        CHECK(step > 38);  // less than the tab's 44: they overlap a little, all still clickable
        CHECK(step < 50);
        CHECK(step == Catch::Approx(tops[1] - tops[0]));
    }
    // Five fit there as they are.
    CHECK(TabTops(5, 30, bottom, 44, 50) == std::vector<float>{30, 80, 130, 180, 230});
}

TEST_CASE("binder: one tab, none", "[binder]") {
    CHECK(TabTops(1, 30, 100, 44, 50) == std::vector<float>{30});
    CHECK(TabTops(0, 30, 100, 44, 50).empty());
    // No room at all: the tabs pile up at the top, never above it.
    CHECK(TabTops(3, 30, 40, 44, 50) == std::vector<float>{30, 30, 30});
}
