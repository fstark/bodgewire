#include "sim/rng.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("rng: fixed seed gives fixed sequence", "[sim]") {
    Rng rng(42);
    CHECK(rng.next() == 0x15780b2e0c2ec716ULL);
    CHECK(rng.next() == 0x6104d9866d113a7eULL);
    CHECK(rng.next() == 0xae17533239e499a1ULL);
    CHECK(rng.next() == 0xecb8ad4703b360a1ULL);
}

TEST_CASE("rng: different seeds differ", "[sim]") {
    Rng a(1);
    Rng b(2);
    REQUIRE(a.next() != b.next());
}
