#include "machine/press.hpp"

#include <catch2/catch_test_macros.hpp>

#include "sim/time.hpp"

using namespace bw;
using namespace bw::literals;

TEST_CASE("press: parses REF:DOWN:UP", "[machine]") {
    const auto press = ParsePress("SW1:200ms:700ms");
    REQUIRE(press.has_value());
    CHECK(press->reference == "SW1");
    CHECK(press->down == 200_ms);
    CHECK(press->up == 700_ms);
}

TEST_CASE("press: bad text", "[machine]") {
    for (const char* text : {"SW1", "SW1:200ms", ":200ms:700ms", "SW1:200ms:700ms:1s",
                             "SW1:soon:700ms", "SW1:700ms:200ms", "SW1:1ms:1ms"}) {
        const auto press = ParsePress(text);
        REQUIRE_FALSE(press.has_value());
        CHECK(press.error() == "bad press '" + std::string(text) + "' (expected REF:DOWN:UP)");
    }
}
