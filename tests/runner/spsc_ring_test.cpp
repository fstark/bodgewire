#include "runner/spsc_ring.hpp"

#include <cstdint>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "sim/recorder.hpp"

using namespace bw;

TEST_CASE("spsc ring: fifo", "[runner]") {
    SpscRing<int> ring(8);
    CHECK(ring.tryPush(1));
    CHECK(ring.tryPush(2));
    CHECK(ring.tryPush(3));
    CHECK(ring.tryPop() == 1);
    CHECK(ring.tryPop() == 2);
    CHECK(ring.tryPop() == 3);
}

TEST_CASE("spsc ring: full and empty", "[runner]") {
    SpscRing<int> ring(4);
    CHECK(ring.empty());
    CHECK_FALSE(ring.tryPop().has_value());
    for (int i = 0; i < 4; ++i) {
        CHECK(ring.tryPush(i));
    }
    CHECK_FALSE(ring.tryPush(4));
    CHECK_FALSE(ring.empty());
    CHECK(ring.tryPop() == 0);
    CHECK(ring.tryPush(4));
    for (int i = 1; i <= 4; ++i) {
        CHECK(ring.tryPop() == i);
    }
    CHECK(ring.empty());
}

TEST_CASE("spsc ring: wrap-around", "[runner]") {
    SpscRing<int> ring(4);
    int next = 0, expected = 0;
    for (int round = 0; round < 1000; ++round) {
        for (int i = 0; i < 3; ++i) {
            REQUIRE(ring.tryPush(next++));
        }
        for (int i = 0; i < 3; ++i) {
            REQUIRE(ring.tryPop() == expected++);
        }
    }
    CHECK(ring.empty());
}

TEST_CASE("spsc ring: capacity must be a power of two", "[runner]") {
    CHECK_THROWS(SpscRing<int>(3));
    CHECK_THROWS(SpscRing<int>(1));
}

TEST_CASE("spsc ring: two threads keep order", "[runner]") {
    constexpr std::int64_t kCount = 10'000'000;
    SpscRing<Sample> ring(1024);
    std::jthread producer([&] {
        for (std::int64_t i = 0; i < kCount; ++i) {
            const Sample sample{i, (i & 1) != 0 ? LogicValue::One : LogicValue::Zero};
            while (!ring.tryPush(sample)) {
                std::this_thread::yield();
            }
        }
    });
    std::int64_t received = 0;
    bool inOrder = true;
    while (received < kCount) {
        if (const auto sample = ring.tryPop()) {
            const LogicValue value = (received & 1) != 0 ? LogicValue::One : LogicValue::Zero;
            inOrder = inOrder && sample->time == received && sample->value == value;
            ++received;
        } else {
            std::this_thread::yield();
        }
    }
    CHECK(inOrder);
    CHECK(ring.empty());
}
