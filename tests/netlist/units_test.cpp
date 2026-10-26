#include "netlist/units.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

TEST_CASE("units: durations", "[netlist]") {
    CHECK(ParseDuration("10ms") == 10_ms);
    CHECK(ParseDuration("250ns") == 250_ns);
    CHECK(ParseDuration("2s") == 2_s);
    CHECK(ParseDuration("1.5us") == 1'500_ns);
    CHECK(ParseDuration("1ps") == 1_ps);
}

TEST_CASE("units: duration errors", "[netlist]") {
    for (const char* bad : {"10", "ms", "10min", "0ns", "", "1.ms"}) {
        INFO(bad);
        CHECK_FALSE(ParseDuration(bad).has_value());
    }
    CHECK(ParseDuration("10min").error() ==
          "bad duration '10min': expected a number then ps, ns, us, ms or s, e.g. 10ms");
    CHECK(ParseDuration("0ns").error() == "duration must be >= 1 ps");
}

TEST_CASE("units: frequency delegates", "[netlist]") {
    CHECK(ParsePeriodFromFrequency("1MHz") == 1_us);
    CHECK_FALSE(ParsePeriodFromFrequency("1GHz").has_value());
}
