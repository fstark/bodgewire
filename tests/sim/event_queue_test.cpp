#include "sim/event_queue.hpp"

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "sim/rng.hpp"

using namespace bw;

namespace {

// The next take, as Events.
std::vector<Event> Take(EventQueue& queue) {
    const Time time = queue.nextTime();
    std::vector<Event> events;
    for (const EventKey key : queue.takeNext()) {
        events.push_back({time, KeyTarget(key), KeyStamp(key)});
    }
    return events;
}

std::vector<Event> Drain(EventQueue& queue) {
    std::vector<Event> events;
    while (!queue.empty()) {
        const std::vector<Event> taken = Take(queue);
        events.insert(events.end(), taken.begin(), taken.end());
    }
    return events;
}

bool Same(const Event& a, const Event& b) {
    return a.time == b.time && a.target == b.target && a.stamp == b.stamp;
}

}  // namespace

TEST_CASE("event queue: pops in time order", "[sim]") {
    EventQueue queue;
    for (Time time : {50, 10, 40, 20, 30}) {
        queue.push({time, 0, 0});
    }
    const std::vector<Event> events = Drain(queue);
    REQUIRE(events.size() == 5);
    for (std::size_t i = 0; i < events.size(); ++i) {
        CHECK(events[i].time == static_cast<Time>(10 * (i + 1)));
    }
}

TEST_CASE("event queue: equal times all before later", "[sim]") {
    EventQueue queue;
    queue.push({20, 1, 0});
    queue.push({10, 5, 0});
    queue.push({20, 0, 0});
    queue.push({10, 3, 0});
    queue.push({10, 9, 0});
    const std::vector<Event> events = Drain(queue);
    REQUIRE(events.size() == 5);
    CHECK(events[0].time == 10);
    CHECK(events[1].time == 10);
    CHECK(events[2].time == 10);
    CHECK(events[3].time == 20);
    CHECK(events[4].time == 20);
}

TEST_CASE("event queue: equal times pop in (target, stamp) order", "[sim]") {
    EventQueue queue;
    queue.push({10, 2, 1});
    queue.push({10, 1, 7});
    queue.push({10, 2, 0});
    queue.push({10, 1, 3});
    const std::vector<Event> events = Drain(queue);
    REQUIRE(events.size() == 4);
    CHECK((events[0].target == 1 && events[0].stamp == 3));
    CHECK((events[1].target == 1 && events[1].stamp == 7));
    CHECK((events[2].target == 2 && events[2].stamp == 0));
    CHECK((events[3].target == 2 && events[3].stamp == 1));
}

TEST_CASE("event queue: clear empties", "[sim]") {
    EventQueue queue;
    queue.push({1, 0, 0});
    queue.push({2, 0, 0});
    queue.clear();
    CHECK(queue.empty());
    CHECK(queue.nextTime() == EventQueue::kNever);
    queue.push({1, 4, 0});
    const std::vector<Event> events = Drain(queue);
    REQUIRE(events.size() == 1);
    CHECK(events[0].target == 4);
}

TEST_CASE("event queue: a take gives every event of the earliest time", "[sim]") {
    EventQueue queue;
    queue.push({10, 2, 0});
    queue.push({10, 1, 0});
    queue.push({20, 3, 0});
    CHECK(queue.nextTime() == 10);
    const std::vector<Event> first = Take(queue);
    REQUIRE(first.size() == 2);
    CHECK(first[0].target == 1);
    CHECK(first[1].target == 2);
    CHECK(queue.nextTime() == 20);
    CHECK(Take(queue).size() == 1);
    CHECK(queue.empty());
    CHECK(queue.nextTime() == EventQueue::kNever);
}

TEST_CASE("event queue: one time pushed in separate runs is one sorted take", "[sim]") {
    EventQueue queue;
    queue.push({10, 5, 0});
    queue.push({20, 1, 0});
    queue.push({10, 3, 0});
    queue.push({30, 0, 0});
    queue.push({10, 4, 0});
    queue.push({10, 2, 0});
    const std::vector<Event> first = Take(queue);
    REQUIRE(first.size() == 4);
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(first[i].target == i + 2);
    }
    CHECK(queue.nextTime() == 20);
}

TEST_CASE("event queue: a far event waits behind near ones", "[sim]") {
    EventQueue queue;
    const Time far = 20'000'000'000;  // a key repeat, 20 ms
    queue.push({far, 99, 0});
    for (Time now = 0; now < 2'000'000; now += 1'000) {  // more pushes than the first buffers hold
        queue.push({now + 9'000, 1, 0});
        queue.push({now + 450'000, 2, 0});
        REQUIRE(queue.nextTime() < far);
        Take(queue);
    }
    const std::vector<Event> rest = Drain(queue);
    REQUIRE(!rest.empty());
    CHECK(rest.back().time == far);
    CHECK(rest.back().target == 99);
}

// Simulator::wake(…, 0) between two runUntil calls: a push earlier than every pending event, or
// for the time just taken.
TEST_CASE("event queue: a push earlier than the pending events is taken first", "[sim]") {
    EventQueue queue;
    queue.push({1'000, 1, 0});
    CHECK(queue.nextTime() == 1'000);
    queue.push({10, 2, 0});
    CHECK(queue.nextTime() == 10);
    CHECK(Take(queue)[0].target == 2);
    queue.push({10, 3, 0});
    CHECK(queue.nextTime() == 10);
    CHECK(Take(queue)[0].target == 3);
    CHECK(Take(queue)[0].target == 1);
    CHECK(queue.empty());
}

TEST_CASE("event queue: more pending times than the first buffer holds", "[sim]") {
    EventQueue queue;
    const Time count = 1'000;
    for (Time i = 0; i < count; ++i) {
        queue.push({(i * 37) % count, static_cast<std::uint32_t>(i), 0});  // every time once
    }
    for (Time time = 0; time < count / 2; ++time) {  // taken runs leave room at the front
        REQUIRE(Take(queue)[0].time == time);
    }
    for (Time i = 0; i < count; ++i) {
        queue.push({count + (i * 37) % count, static_cast<std::uint32_t>(i), 0});
    }
    const std::vector<Event> events = Drain(queue);
    REQUIRE(events.size() == static_cast<std::size_t>(count + count / 2));
    for (std::size_t i = 0; i < events.size(); ++i) {
        REQUIRE(events[i].time == count / 2 + static_cast<Time>(i));
    }
}

// Against a sorted vector, through many refills of the queue's buffers: bursts for one time (the
// open run growing across a refill), repeated times, clears.
TEST_CASE("event queue: same takes as a sorted list over long random use", "[sim]") {
    const auto before = [](const Event& a, const Event& b) {
        return std::tie(a.time, a.target, a.stamp) < std::tie(b.time, b.target, b.stamp);
    };
    constexpr Time kDelays[] = {1,      7,      9'000,   9'000,         15'000,
                                22'000, 77'000, 450'000, 20'000'000'000};
    Rng rng(1);
    EventQueue queue;
    std::vector<Event> expected;  // pending, unsorted
    Time now = 0;
    for (int step = 0; step < 60'000; ++step) {
        Time delay = 0;
        for (std::uint64_t pushes = rng.next() % (expected.size() < 40 ? 12 : 3); pushes > 0;
             --pushes) {
            if (delay == 0 || rng.next() % 3 != 0) {  // one time in three: the delay before
                delay = kDelays[rng.next() % std::size(kDelays)];
            }
            const Event event{now + delay, static_cast<std::uint32_t>(rng.next() % 300),
                              static_cast<std::uint32_t>(rng.next() % 64)};
            queue.push(event);
            expected.push_back(event);
        }
        for (std::uint64_t takes = rng.next() % 4; takes > 0 && !expected.empty(); --takes) {
            std::ranges::sort(expected, before);
            now = expected.front().time;
            REQUIRE(queue.nextTime() == now);
            const std::vector<Event> taken = Take(queue);
            const auto end =
                std::ranges::find_if(expected, [&](const Event& e) { return e.time != now; });
            REQUIRE(taken.size() == static_cast<std::size_t>(end - expected.begin()));
            for (std::size_t i = 0; i < taken.size(); ++i) {
                REQUIRE(Same(taken[i], expected[i]));
            }
            expected.erase(expected.begin(), end);
        }
        REQUIRE(queue.empty() == expected.empty());
        if (rng.next() % 4'000 == 0) {
            queue.clear();
            expected.clear();
        }
    }
}

TEST_CASE("event queue: stamp round-trips", "[sim]") {
    for (std::uint32_t generation : {0u, 1u, 12345u, kGenerationMask}) {
        for (bool isWake : {false, true}) {
            for (LogicValue value :
                 {LogicValue::Zero, LogicValue::One, LogicValue::Z, LogicValue::X}) {
                const std::uint32_t stamp = MakeStamp(generation, isWake, value);
                CHECK(StampGeneration(stamp) == generation);
                CHECK(StampIsWake(stamp) == isWake);
                CHECK(StampValue(stamp) == value);
            }
        }
    }
}

TEST_CASE("event queue: order holds across the whole range of each field", "[sim]") {
    EventQueue queue;
    const Time late = Time{1} << 50;
    queue.push({late, 0, 0});
    queue.push({late - 1, UINT32_MAX, UINT32_MAX});
    queue.push({0, UINT32_MAX, 0});
    queue.push({0, 0, UINT32_MAX});
    queue.push({late, 0, 1});
    const std::vector<Event> events = Drain(queue);
    REQUIRE(events.size() == 5);
    CHECK((events[0].time == 0 && events[0].target == 0));
    CHECK((events[1].time == 0 && events[1].target == UINT32_MAX));
    CHECK(events[2].time == late - 1);
    CHECK((events[3].time == late && events[3].stamp == 0));
    CHECK((events[4].time == late && events[4].stamp == 1));
}
