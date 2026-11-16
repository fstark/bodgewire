#include "parts/press_queue.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

TEST_CASE("press queue: a press is held, then released, then the next", "[parts]") {
    PressQueue queue(40_ms, 40_ms);
    CHECK_FALSE(queue.current().has_value());
    CHECK_FALSE(queue.step(1_ms).has_value());
    REQUIRE(queue.push({3}));
    REQUIRE(queue.push({5, true}));
    // Taken at the next step: held 40 ms.
    CHECK(queue.step(2_ms) == 40_ms);
    REQUIRE(queue.current().has_value());
    CHECK(queue.current()->key == 3);
    CHECK_FALSE(queue.current()->shift);
    CHECK(queue.scripted(3, 9));
    CHECK_FALSE(queue.scripted(9, 9));
    // A step before the hold's end changes nothing.
    CHECK_FALSE(queue.step(20_ms).has_value());
    CHECK(queue.current()->key == 3);
    // The hold ends: released 40 ms.
    CHECK(queue.step(42_ms) == 40_ms);
    CHECK_FALSE(queue.current().has_value());
    CHECK_FALSE(queue.scripted(3, 9));
    // The gap ends: the next press, with SHIFT.
    CHECK(queue.step(82_ms) == 40_ms);
    CHECK(queue.current()->key == 5);
    CHECK(queue.current()->shift);
    CHECK(queue.scripted(5, 9));
    CHECK(queue.scripted(9, 9));
    CHECK(queue.step(122_ms) == 40_ms);
    CHECK_FALSE(queue.step(162_ms).has_value());  // idle, nothing waiting
}

TEST_CASE("press queue: dropped presses are skipped, a full queue refuses", "[parts]") {
    PressQueue queue(10_ms, 10_ms);
    REQUIRE(queue.push({1}));
    REQUIRE(queue.push({2}));
    queue.dropWaiting();
    REQUIRE(queue.push({3}));
    CHECK(queue.step(0) == 10_ms);
    CHECK(queue.current()->key == 3);
    queue.reset();
    CHECK_FALSE(queue.current().has_value());
    for (std::size_t i = 0; i < kMatrixWaiting; ++i) {
        REQUIRE(queue.push({i}));
    }
    CHECK_FALSE(queue.push({0}));
}
