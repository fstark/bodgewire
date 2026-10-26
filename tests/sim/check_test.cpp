#include "sim/check.hpp"

#include <stdexcept>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace bw;

TEST_CASE("check: throws logic_error with message", "[sim]") {
    try {
        Check(false, "boom");
        FAIL("Check did not throw");
    } catch (const std::logic_error& error) {
        const std::string text = error.what();
        REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("check_test.cpp:"));
        REQUIRE_THAT(text, Catch::Matchers::EndsWith(": boom"));
    }
}

TEST_CASE("check: passes silently", "[sim]") { REQUIRE_NOTHROW(Check(true, "fine")); }
