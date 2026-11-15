#include "sim/logic.hpp"

#include <initializer_list>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

using enum LogicValue;
using enum DriveStrength;

Resolution Resolve(std::initializer_list<Drive> drives, bool hasTtlInput = false,
                   LogicValue lastDriven = Zero) {
    Resolver resolver;
    for (const Drive& drive : drives) {
        resolver.add(drive);
    }
    return resolver.finish(hasTtlInput, lastDriven);
}

constexpr Drive Ttl(LogicValue value, DriveStrength strength = Strong) {
    return {value, strength, Family::Ttl};
}

constexpr Drive Mos(LogicValue value, DriveStrength strength = Strong) {
    return {value, strength, Family::Mos};
}

bool Same(Drive a, Drive b) {
    return a.value == b.value && a.strength == b.strength && a.family == b.family;
}

}  // namespace

TEST_CASE("logic: DriveOf", "[sim]") {
    const Family f = Family::Mos;
    CHECK(Same(DriveOf(PinKind::Output, f, Zero), Mos(Zero)));
    CHECK(Same(DriveOf(PinKind::Output, f, One), Mos(One)));
    CHECK(Same(DriveOf(PinKind::Output, f, X), Mos(X)));
    CHECK(Same(DriveOf(PinKind::Output, f, Z), Mos(Z, None)));

    CHECK(Same(DriveOf(PinKind::OpenCollector, f, Zero), Mos(Zero)));
    CHECK(Same(DriveOf(PinKind::OpenCollector, f, One), Mos(Z, None)));
    CHECK(Same(DriveOf(PinKind::OpenCollector, f, X), Mos(X)));
    CHECK(Same(DriveOf(PinKind::OpenCollector, f, Z), Mos(Z, None)));

    for (LogicValue value : {Zero, One, Z, X}) {
        CHECK(Same(DriveOf(PinKind::Input, f, value), Mos(Z, None)));
    }
}

TEST_CASE("logic: IsDriver", "[sim]") {
    CHECK_FALSE(IsDriver(PinKind::Input));
    CHECK(IsDriver(PinKind::Output));
    CHECK(IsDriver(PinKind::OpenCollector));
    CHECK(IsDriver(PinKind::Bidirectional));
    CHECK(IsDriver(PinKind::PullUp));
    CHECK_FALSE(IsDriver(PinKind::Power));
    CHECK_FALSE(IsDriver(PinKind::NoConnect));
}

TEST_CASE("logic: IsReader", "[sim]") {
    CHECK(IsReader(PinKind::Input));
    CHECK(IsReader(PinKind::Bidirectional));
    CHECK(IsReader(PinKind::PullUp));
    CHECK_FALSE(IsReader(PinKind::Output));
    CHECK_FALSE(IsReader(PinKind::OpenCollector));
    CHECK_FALSE(IsReader(PinKind::Power));
    CHECK_FALSE(IsReader(PinKind::NoConnect));
}

TEST_CASE("logic: DriveOf bidirectional is a tri-state output", "[sim]") {
    const Family f = Family::Mos;
    CHECK(Same(DriveOf(PinKind::Bidirectional, f, Zero), Mos(Zero)));
    CHECK(Same(DriveOf(PinKind::Bidirectional, f, One), Mos(One)));
    CHECK(Same(DriveOf(PinKind::Bidirectional, f, X), Mos(X)));
    CHECK(Same(DriveOf(PinKind::Bidirectional, f, Z), Mos(Z, None)));
}

TEST_CASE("logic: DriveOf pull-up drives 0 at port strength and 1 weakly", "[sim]") {
    const Family f = Family::Mos;
    CHECK(Same(DriveOf(PinKind::PullUp, f, Zero), Mos(Zero, Port)));
    CHECK(Same(DriveOf(PinKind::PullUp, f, One), Mos(One, Weak)));
    CHECK(Same(DriveOf(PinKind::PullUp, f, X), Mos(X, Port)));
    CHECK(Same(DriveOf(PinKind::PullUp, f, Z), Mos(Z, None)));
    // Its 1 loses to any strong 0 and reads 1 alone, not floating.
    CHECK(Resolve({DriveOf(PinKind::PullUp, f, One), Ttl(Zero)}).value == Zero);
    const Resolution alone = Resolve({DriveOf(PinKind::PullUp, f, One)});
    CHECK(alone.value == One);
    CHECK_FALSE(alone.floating);
}

TEST_CASE("resolve: a port's 0 loses to an output's 1", "[sim]") {
    const Drive port0 = DriveOf(PinKind::PullUp, Family::Mos, Zero);
    CHECK(Resolve({port0, Ttl(One)}).value == One);
    CHECK(Resolve({port0, Mos(One)}).value == One);
    CHECK(Resolve({DriveOf(PinKind::PullUp, Family::Mos, X), Ttl(One)}).value == One);
    CHECK(Resolve({port0, Ttl(Zero)}).value == Zero);
}

TEST_CASE("resolve: a port's 0 beats a pull-up and another port's 1", "[sim]") {
    const Drive port0 = DriveOf(PinKind::PullUp, Family::Mos, Zero);
    const Drive port1 = DriveOf(PinKind::PullUp, Family::Mos, One);
    CHECK(Resolve({port0, Ttl(One, Weak)}).value == Zero);
    CHECK(Resolve({port0, port1}).value == Zero);
    CHECK(Resolve({port1, port0, port1}).value == Zero);
    CHECK(Resolve({port0, port0}).value == Zero);
}

TEST_CASE("logic: DriveOf power and no-connect is None", "[sim]") {
    for (LogicValue value : {Zero, One, Z, X}) {
        CHECK(Same(DriveOf(PinKind::Power, Family::Ttl, value), Ttl(Z, None)));
        CHECK(Same(DriveOf(PinKind::NoConnect, Family::Ttl, value), Ttl(Z, None)));
    }
}

TEST_CASE("resolve: single strong driver", "[sim]") {
    const Resolution r = Resolve({Ttl(One)});
    CHECK(r.value == One);
    CHECK_FALSE(r.floating);
}

TEST_CASE("resolve: strong beats weak", "[sim]") {
    CHECK(Resolve({Ttl(One, Weak), Ttl(Zero)}).value == Zero);
    CHECK(Resolve({Ttl(Zero), Ttl(One, Weak)}).value == Zero);
}

TEST_CASE("resolve: supply beats strong", "[sim]") {
    CHECK(Resolve({Ttl(Zero), Ttl(One, Supply)}).value == One);
}

TEST_CASE("resolve: weak alone", "[sim]") {
    const Resolution r = Resolve({Ttl(One, Weak)}, true);
    CHECK(r.value == One);
    CHECK_FALSE(r.floating);
}

TEST_CASE("resolve: no driver with TTL input reads 1, floating", "[sim]") {
    const Resolution r = Resolve({Ttl(Z, None)}, true, Zero);
    CHECK(r.value == One);
    CHECK(r.floating);
}

TEST_CASE("resolve: no driver MOS-only keeps last driven, floating", "[sim]") {
    const Resolution r = Resolve({Mos(Z, None)}, false, Zero);
    CHECK(r.value == Zero);
    CHECK(r.floating);
}

TEST_CASE("resolve: TTL contention reads 0", "[sim]") {
    const Resolution r = Resolve({Ttl(One), Ttl(Zero)});
    CHECK(r.value == Zero);
    CHECK_FALSE(r.floating);
}

TEST_CASE("resolve: mixed-family contention reads X", "[sim]") {
    CHECK(Resolve({Ttl(One), Mos(Zero)}).value == X);
}

TEST_CASE("resolve: supply contention reads X", "[sim]") {
    CHECK(Resolve({Ttl(One, Supply), Ttl(Zero, Supply)}).value == X);
}

TEST_CASE("resolve: X driver gives X", "[sim]") {
    CHECK(Resolve({Ttl(One), Ttl(X)}).value == X);
    CHECK(Resolve({Ttl(X)}).value == X);
}

TEST_CASE("resolve: weaker X is ignored", "[sim]") {
    CHECK(Resolve({Ttl(X, Weak), Ttl(One)}).value == One);
    CHECK(Resolve({Ttl(One), Ttl(X, Weak)}).value == One);
}

TEST_CASE("resolve: open-collector wired-AND with weak pull-up", "[sim]") {
    const Drive pullUp = Ttl(One, Weak);
    const auto oc = [](LogicValue value) {
        return DriveOf(PinKind::OpenCollector, Family::Ttl, value);
    };
    CHECK(Resolve({pullUp, oc(One), oc(One), oc(One)}, true).value == One);
    CHECK(Resolve({pullUp, oc(One), oc(Zero), oc(One)}, true).value == Zero);
}
