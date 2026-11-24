#include "machine/knob_setting.hpp"

#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

TEST_CASE("knob setting: parses REF:N%", "[machine]") {
    const auto knob = ParseKnob("J2:40%");
    REQUIRE(knob.has_value());
    CHECK(knob->reference == "J2");
    CHECK(knob->turn == 0.4);
    CHECK(ParseKnob("J2:0%")->turn == 0.0);
    CHECK(ParseKnob("J2:100%")->turn == 1.0);
    CHECK(ParseKnob("J3:12.5%")->turn == 0.125);
}

TEST_CASE("knob setting: bad text", "[machine]") {
    for (const char* text : {"J2", ":40%", "J2:40", "J2:x%", "J2:101%", "J2:4.%", "J2:%"}) {
        const auto knob = ParseKnob(text);
        REQUIRE_FALSE(knob.has_value());
        CHECK(knob.error() ==
              "bad knob '" + std::string(text) + "' (expected REF:N% or REF:N%:TIME, 0 to 100)");
    }
}

TEST_CASE("knob setting: a time", "[machine]") {
    const auto untimed = ParseKnob("J2:40%");
    REQUIRE(untimed.has_value());
    CHECK(untimed->at == 0);  // from power-on
    const auto knob = ParseKnob("J2:87%:400ms");
    REQUIRE(knob.has_value());
    CHECK(knob->reference == "J2");
    CHECK(knob->turn == 0.87);
    CHECK(knob->at == 400_ms);
    CHECK(ParseKnob("J2:5%:1.5s")->at == 1500_ms);
}

TEST_CASE("knob setting: a bad time", "[machine]") {
    for (const char* text : {"J2:40%:", "J2:40%:soon", "J2:40%:0ms", "J2:40%:1ms:2ms", "J2::1ms"}) {
        CHECK_FALSE(ParseKnob(text).has_value());
    }
}
