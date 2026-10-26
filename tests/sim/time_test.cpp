#include "sim/time.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

TEST_CASE("time: literals", "[sim]") {
    STATIC_REQUIRE(1_ps == 1);
    STATIC_REQUIRE(1_ns == 1000);
    STATIC_REQUIRE(1_us == 1000_ns);
    STATIC_REQUIRE(1_ms == 1000_us);
    STATIC_REQUIRE(1_s == kPicosPerSecond);
}
