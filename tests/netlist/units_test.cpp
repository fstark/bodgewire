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
    CHECK(ParseFrequencyPeriod("1MHz") == 1_us);
    CHECK_FALSE(ParseFrequencyPeriod("1GHz").has_value());
}

TEST_CASE("units: ohms", "[netlist]") {
    CHECK(ParseOhms("470") == 470u);
    CHECK(ParseOhms("470R") == 470u);
    CHECK(ParseOhms("4R7") == 5u);
    CHECK(ParseOhms("330") == 330u);
    CHECK(ParseOhms("1k") == 1'000u);
    CHECK(ParseOhms("1K") == 1'000u);
    CHECK(ParseOhms("2k2") == 2'200u);
    CHECK(ParseOhms("2k4") == 2'400u);
    CHECK(ParseOhms("3k3") == 3'300u);
    CHECK(ParseOhms("4k7") == 4'700u);
    CHECK(ParseOhms("10k") == 10'000u);
    CHECK(ParseOhms("47K") == 47'000u);
    CHECK(ParseOhms("220k") == 220'000u);
    CHECK(ParseOhms("1M") == 1'000'000u);
}

TEST_CASE("units: ohms errors", "[netlist]") {
    for (const char* bad : {"", "k", "1x", "1kk", "1k2345", "0", "1 k", "-1", "1.5k"}) {
        INFO(bad);
        CHECK_FALSE(ParseOhms(bad).has_value());
    }
    CHECK(ParseOhms("1q").error() == "bad resistance '1q': expected e.g. 470, 1k, 4k7 or 1M");
}

TEST_CASE("units: farads", "[netlist]") {
    CHECK(ParseFarads("47p") == 47u);
    CHECK(ParseFarads("4p7") == 5u);
    CHECK(ParseFarads("330p") == 330u);
    CHECK(ParseFarads("100n") == 100'000u);
    CHECK(ParseFarads("4n7") == 4'700u);
    CHECK(ParseFarads("1u") == 1'000'000u);
    CHECK(ParseFarads("2u2") == 2'200'000u);
    const std::string expected = "': expected e.g. 47p, 100n, 4n7 or 1u";
    for (const char* bad : {"", "47", "p", "47F", "4.7n", "1uu", "1m", "0p"}) {
        INFO(bad);
        REQUIRE_FALSE(ParseFarads(bad).has_value());
        CHECK(ParseFarads(bad).error() == "bad capacitance '" + std::string(bad) + expected);
    }
}
