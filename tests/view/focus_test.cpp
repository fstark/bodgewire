#include "view/focus.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("focus: the probe owns it at start, nothing out", "[view]") {
    const Focus focus;
    CHECK(focus.owner() == Owner::Probe);
    CHECK_FALSE(focus.out(Owner::Analyzer));
    CHECK_FALSE(focus.out(Owner::Scope));
}

TEST_CASE("focus: the meter is taken like the probe", "[view]") {
    Focus focus;
    CHECK_FALSE(IsInstrument(Owner::Meter));
    CHECK(Measures(Owner::Meter));
    focus.take(Owner::Meter);
    CHECK(focus.owner() == Owner::Meter);
    for (std::size_t i = 0; i < kBenchObjects; ++i) {
        CHECK_FALSE(focus.out(static_cast<BenchObject>(i)));
    }
    focus.take(Owner::Iron);
    CHECK(focus.owner() == Owner::Iron);
}

TEST_CASE("focus: a tool taken owns it, the instruments stay out", "[view]") {
    Focus focus;
    focus.bringOutFocused(Owner::Scope);
    focus.take(Owner::Iron);
    CHECK(focus.owner() == Owner::Iron);
    CHECK(focus.out(Owner::Scope));
}

TEST_CASE("focus: an instrument's key brings it out with the focus, never away", "[view]") {
    Focus focus;
    focus.bringOutFocused(Owner::Analyzer);
    CHECK(focus.owner() == Owner::Analyzer);
    CHECK(focus.out(Owner::Analyzer));
    focus.bringOutFocused(Owner::Analyzer);
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
    focus.bringOut(Owner::Scope);
    CHECK(focus.owner() == Owner::Probe);
    focus.clicked(Owner::Scope);
    CHECK(focus.owner() == Owner::Scope);
    focus.clicked(Owner::Analyzer);  // away: no window to click
    CHECK(focus.owner() == Owner::Scope);
}

TEST_CASE("focus: closing a window puts it away, the probe takes the focus from it", "[view]") {
    Focus focus;
    focus.bringOut(Owner::Analyzer);
    focus.bringOutFocused(Owner::Scope);
    focus.closed(Owner::Analyzer);  // not the owner
    CHECK(focus.owner() == Owner::Scope);
    CHECK_FALSE(focus.out(Owner::Analyzer));
    focus.closed(Owner::Scope);
    CHECK(focus.owner() == Owner::Probe);
    CHECK_FALSE(focus.out(Owner::Scope));
}

TEST_CASE("focus: the pencil is a tool", "[view]") {
    Focus focus;
    CHECK_FALSE(IsInstrument(Owner::Pencil));
    focus.take(Owner::Pencil);
    CHECK(focus.owner() == Owner::Pencil);
    focus.take(Owner::Probe);
    CHECK(focus.owner() == Owner::Probe);
}

TEST_CASE("focus: one out-or-away for every bench object", "[view]") {
    Focus focus;
    focus.outFlag(BenchObject::Tv) = true;
    CHECK(focus.out(BenchObject::Tv));
    CHECK_FALSE(focus.out(BenchObject::Scope));
    focus.bringOutFocused(Owner::Scope);
    CHECK(focus.out(BenchObject::Scope));  // the instrument's own, the same store
    CHECK(focus.out(Owner::Scope));
    focus.closed(Owner::Scope);
    CHECK_FALSE(focus.out(BenchObject::Scope));
    CHECK(focus.owner() == Owner::Probe);
    CHECK(focus.out(BenchObject::Tv));  // untouched
    CHECK(ObjectOf(Owner::Analyzer) == BenchObject::Analyzer);
}

TEST_CASE("focus: an instrument is not put away behind the focus's back", "[view]") {
    Focus focus;
    focus.bringOut(BenchObject::Scope);  // --pop: out, the owner unchanged
    CHECK(focus.out(Owner::Scope));
    CHECK(focus.owner() == Owner::Probe);
    focus.bringOutFocused(Owner::Scope);
    CHECK_THROWS(focus.outFlag(BenchObject::Scope) = false);  // closed() is the way
    CHECK(focus.out(Owner::Scope));
    CHECK(focus.owner() == Owner::Scope);
    focus.outFlag(BenchObject::Debug) = true;
    focus.outFlag(BenchObject::Debug) = false;
    CHECK_FALSE(focus.out(BenchObject::Debug));
}

TEST_CASE("focus: a link to the Board measures or frames", "[view]") {
    const BoardTarget pin{{3}, 7, std::nullopt, {}};
    const BoardTarget part{{3}, std::nullopt, std::nullopt, {}};
    const BoardTarget run{{3, 4}, std::nullopt, std::nullopt, {}};
    const BoardTarget net{{}, std::nullopt, NetId{2}, {}};
    CHECK(RouteLink(Owner::Probe, pin, false, false) == LinkAction::Measure);
    CHECK(RouteLink(Owner::Scope, pin, false, false) == LinkAction::Measure);
    CHECK(RouteLink(Owner::Meter, pin, false, false) == LinkAction::Measure);
    CHECK(RouteLink(Owner::Probe, pin, false, true) == LinkAction::Frame);           // double-click
    CHECK(RouteLink(Owner::Probe, pin, true, false) == LinkAction::FrameAndSelect);  // Cmd
    CHECK(RouteLink(Owner::Puller, pin, false, false) == LinkAction::FrameAndSelect);
    CHECK(RouteLink(Owner::Probe, part, false, false) == LinkAction::FrameAndSelect);
    CHECK(RouteLink(Owner::Probe, run, false, false) == LinkAction::Frame);
    CHECK(RouteLink(Owner::Probe, net, false, false) == LinkAction::Frame);
}
