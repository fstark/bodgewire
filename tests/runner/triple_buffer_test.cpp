#include "runner/triple_buffer.hpp"

#include <array>
#include <cstdint>
#include <thread>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("triple buffer: nothing before publish", "[runner]") {
    TripleBuffer<int> buffer;
    CHECK_FALSE(buffer.update());
}

TEST_CASE("triple buffer: reader gets the latest", "[runner]") {
    TripleBuffer<int> buffer;
    buffer.back() = 1;
    buffer.publish();
    CHECK(buffer.update());
    CHECK(buffer.front() == 1);
    CHECK_FALSE(buffer.update());
    CHECK(buffer.front() == 1);

    buffer.back() = 2;
    buffer.publish();
    buffer.back() = 3;
    buffer.publish();
    CHECK(buffer.update());
    CHECK(buffer.front() == 3);
    CHECK_FALSE(buffer.update());
}

TEST_CASE("triple buffer: never torn", "[runner]") {
    using Block = std::array<std::uint64_t, 64>;
    constexpr std::uint64_t kCount = 200'000;
    TripleBuffer<Block> buffer;
    std::jthread writer([&] {
        for (std::uint64_t i = 1; i <= kCount; ++i) {
            buffer.back().fill(i);
            buffer.publish();
        }
    });
    std::uint64_t last = 0;
    bool consistent = true, increasing = true;
    while (last < kCount) {
        if (!buffer.update()) {
            std::this_thread::yield();
            continue;
        }
        const Block& block = buffer.front();
        for (const std::uint64_t value : block) {
            consistent = consistent && value == block[0];
        }
        increasing = increasing && block[0] > last;
        last = block[0];
    }
    CHECK(consistent);
    CHECK(increasing);
}
