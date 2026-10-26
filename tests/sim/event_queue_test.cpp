#include "sim/event_queue.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

std::vector<Event> Drain(EventQueue& queue) {
    std::vector<Event> events;
    while (!queue.empty()) {
        events.push_back(queue.top());
        queue.pop();
    }
    return events;
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
