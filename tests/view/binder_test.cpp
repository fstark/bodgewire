#include "view/binder.hpp"

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
