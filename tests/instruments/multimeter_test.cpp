#include <catch2/catch_test_macros.hpp>

#include "instruments/volts.hpp"

using namespace bw;

TEST_CASE("volts: logic states", "[instruments]") {
    CHECK(LogicVolts({0, LogicValue::Zero}) == kLowVolts);
    CHECK(LogicVolts({0, LogicValue::One}) == kHighVolts);
    CHECK(LogicVolts({0, LogicValue::Z, true}) == kFloatingVolts);
    CHECK(LogicVolts({0, LogicValue::One, true}) == kFloatingVolts);
    CHECK(LogicVolts({0, LogicValue::X}) == kUnknownVolts);
    CHECK(kLowVolts == 0.2);
    CHECK(kHighVolts == 3.5);
    CHECK(kFloatingVolts == 1.4);
    CHECK(kUnknownVolts == 1.0);
}
