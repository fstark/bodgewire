#include "view/focus.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("focus: the probe owns it at start, nothing out", "[view]") {
    const Focus focus;
    CHECK(focus.owner() == Owner::Probe);
    CHECK_FALSE(focus.out(Owner::Analyzer));
    CHECK_FALSE(focus.out(Owner::Scope));
    CHECK_FALSE(focus.out(Owner::Meter));
}

TEST_CASE("focus: a tool taken owns it, the instruments stay out", "[view]") {
    Focus focus;
    focus.key(Owner::Scope);
    focus.take(Owner::Iron);
    CHECK(focus.owner() == Owner::Iron);
    CHECK(focus.out(Owner::Scope));
}

TEST_CASE("focus: an instrument's key brings it out with the focus, never away", "[view]") {
    Focus focus;
    focus.key(Owner::Analyzer);
    CHECK(focus.owner() == Owner::Analyzer);
    CHECK(focus.out(Owner::Analyzer));
    focus.key(Owner::Analyzer);
    CHECK(focus.owner() == Owner::Analyzer);
    CHECK(focus.out(Owner::Analyzer));
}

TEST_CASE("focus: a shelf click brings out, focuses, then puts away", "[view]") {
    Focus focus;
    focus.shelfClick(Owner::Scope, false);  // away
    CHECK(focus.owner() == Owner::Scope);
    CHECK(focus.out(Owner::Scope));
    focus.take(Owner::Wire);
    focus.shelfClick(Owner::Scope, true);  // out and in front, without the focus
    CHECK(focus.owner() == Owner::Scope);
    CHECK(focus.out(Owner::Scope));
    focus.shelfClick(Owner::Scope, false);  // the owner, behind another window: stays out
    CHECK(focus.owner() == Owner::Scope);
    CHECK(focus.out(Owner::Scope));
    focus.shelfClick(Owner::Scope, true);  // the owner, in front
    CHECK(focus.owner() == Owner::Probe);
    CHECK_FALSE(focus.out(Owner::Scope));
}

TEST_CASE("focus: a click in an out instrument's window gives it the focus", "[view]") {
    Focus focus;
    focus.bringOut(Owner::Meter);
    CHECK(focus.owner() == Owner::Probe);
    focus.clicked(Owner::Meter);
    CHECK(focus.owner() == Owner::Meter);
    focus.clicked(Owner::Analyzer);  // away: no window to click
    CHECK(focus.owner() == Owner::Meter);
}

TEST_CASE("focus: closing a window puts it away, the probe takes the focus from it", "[view]") {
    Focus focus;
    focus.bringOut(Owner::Analyzer);
    focus.key(Owner::Scope);
    focus.closed(Owner::Analyzer);  // not the owner
    CHECK(focus.owner() == Owner::Scope);
    CHECK_FALSE(focus.out(Owner::Analyzer));
    focus.closed(Owner::Scope);
    CHECK(focus.owner() == Owner::Probe);
    CHECK_FALSE(focus.out(Owner::Scope));
}

TEST_CASE("focus: Esc steps back", "[view]") {
    Focus focus;
    focus.take(Owner::Wire);
    CHECK(focus.escape(true) == EscapeStep::CancelWire);
    CHECK(focus.owner() == Owner::Wire);
    focus.key(Owner::Scope);
    CHECK(focus.escape(false) == EscapeStep::ToProbe);
    CHECK(focus.owner() == Owner::Probe);
    CHECK(focus.out(Owner::Scope));
    CHECK(focus.escape(false) == EscapeStep::LiftTip);
    CHECK(focus.owner() == Owner::Probe);
}
