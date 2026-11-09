#include "machine/knob_setting.hpp"

#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

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
        CHECK(knob.error() == "bad knob '" + std::string(text) + "' (expected REF:N%, 0 to 100)");
    }
}
