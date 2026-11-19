#include "view/power_cycle.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("power cycle: a click that would repair nothing does nothing", "[view]") {
    for (const bool powered : {false, true}) {
        for (const bool dontAsk : {false, true}) {
            CHECK_FALSE(StepFor(false, powered, dontAsk).has_value());
        }
    }
}

TEST_CASE("power cycle: powered off, the repair at once", "[view]") {
    CHECK(StepFor(true, false, false) == RepairStep::Apply);
    CHECK(StepFor(true, false, true) == RepairStep::Apply);
}

TEST_CASE("power cycle: powered on, ask, or cycle once told not to ask", "[view]") {
    CHECK(StepFor(true, true, false) == RepairStep::Ask);
    CHECK(StepFor(true, true, true) == RepairStep::Cycle);
}
