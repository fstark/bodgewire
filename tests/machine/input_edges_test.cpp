#include "machine/input_edges.hpp"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "parts/paddle.hpp"
#include "parts/switch.hpp"
#include "runner/led_input.hpp"
#include "session/session.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

std::unique_ptr<LoadedCircuit> Build(std::string_view machine) {
    auto loaded = LoadNetlistFile(test::NetlistPath(machine));
    REQUIRE(loaded.has_value());
    return std::move(*loaded);
}

std::vector<InputEdge> Edges(LoadedCircuit& loaded, std::vector<ButtonPress> presses,
                             std::vector<KnobSetting> knobs = {}, std::vector<Typing> typing = {}) {
    auto edges = InputEdges(loaded, presses, knobs, typing);
    REQUIRE(edges.has_value());
    return std::move(*edges);
}

}  // namespace

TEST_CASE("input edges: a press is two edges", "[machine]") {
    const auto loaded = Build("dice");
    const auto edges = Edges(*loaded, {{"SW1", 2_ms, 5_ms}});
    REQUIRE(edges.size() == 2);
    Switch* button = SwitchOf(*loaded, "SW1");
    CHECK(edges[0].at == 2_ms);
    CHECK(edges[0].wake == button->id());
    CHECK(edges[1].at == 5_ms);
    CHECK_FALSE(button->pressed());
    edges[0].apply();
    CHECK(button->pressed());
    edges[1].apply();
    CHECK_FALSE(button->pressed());
}

TEST_CASE("input edges: edges in time order", "[machine]") {
    const auto loaded = Build("stopwatch");
    const auto edges =
        Edges(*loaded, {{"SW1", 30_ms, 40_ms}, {"SW2", 5_ms, 35_ms}, {"SW3", 10_ms, 20_ms}});
    REQUIRE(edges.size() == 6);
    const std::vector<Time> expected{5_ms, 10_ms, 20_ms, 30_ms, 35_ms, 40_ms};
    for (std::size_t i = 0; i < edges.size(); ++i) {
        CHECK(edges[i].at == expected[i]);
    }
}

TEST_CASE("input edges: an untimed knob is set at once", "[machine]") {
    const auto loaded = Build("knob");
    const Paddle* paddle = PaddleOf(*loaded, "J2");
    REQUIRE(paddle != nullptr);
    CHECK(paddle->knob() == 0.0);
    CHECK(Edges(*loaded, {}, {{"J2", 0.42}}).empty());
    CHECK(paddle->knob() == 0.42);
}

TEST_CASE("input edges: a timed knob turns the paddle then", "[machine]") {
    // The Knob meter reads the knob about every 10 ms: 42 at 42 %, 86 at 87 %.
    const auto loaded = Build("knob");
    const Paddle* paddle = PaddleOf(*loaded, "J2");
    const auto edges = Edges(*loaded, {}, {{"J2", 0.87, 400_ms}, {"J2", 0.42}});
    REQUIRE(edges.size() == 1);
    CHECK(edges[0].at == 400_ms);
    CHECK(paddle->knob() == 0.42);

    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    LedInput leds(sim, FindLedNets(*loaded));
    const auto slice = [&] {
        leds.update();
        sim.trimRecordings();
        return true;
    };
    const auto lamps = [&] {
        std::vector<float> lit;
        for (std::size_t i = 0; i < leds.leds().size(); ++i) {
            lit.push_back(leds.brightness(i));
        }
        return lit;
    };
    std::size_t next = 0;
    REQUIRE(RunTo(sim, edges, next, 300_ms, slice));
    CHECK(next == 0);
    CHECK(paddle->knob() == 0.42);
    const std::vector<float> at42 = lamps();
    REQUIRE(RunTo(sim, edges, next, 700_ms, slice));
    CHECK(next == 1);
    CHECK(paddle->knob() == 0.87);
    const std::vector<float> at87 = lamps();
    REQUIRE(at42.size() == 14);  // two digits, no decimal points
    // 42: tens b c f g, units a b d e g; 86: tens all seven, units c d e f g.
    CHECK(at42 == std::vector<float>{0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1});
    CHECK(at87 == std::vector<float>{1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1});
}

TEST_CASE("input edges: text on a key matrix is one edge", "[machine]") {
    const auto loaded = Build("bw80");  // built, not run
    const auto typing = ParseTyping("J3:350ms:PRINT 42\\n");
    REQUIRE(typing.has_value());
    const auto edges = Edges(*loaded, {}, {}, {*typing});
    REQUIRE(edges.size() == 1);
    CHECK(edges[0].at == 350_ms);
}

TEST_CASE("input edges: text on a keyboard is an edge a key", "[machine]") {
    const auto loaded = Build("typewriter");
    const auto typing = ParseTyping("J3:1s:HELLO");
    REQUIRE(typing.has_value());
    const auto edges = Edges(*loaded, {}, {}, {*typing});
    REQUIRE(edges.size() == 5);
    CHECK(edges[0].at == 1000_ms);
    CHECK(edges[4].at == 1000_ms + 4 * kTypingInterval);
}

TEST_CASE("input edges: an unknown reference", "[machine]") {
    const auto loaded = Build("dice");
    const std::vector<ButtonPress> presses{{"SW9", 1_ms, 2_ms}};
    const std::vector<KnobSetting> knobs{{"J9", 0.5}};
    const auto typing = ParseTyping("J9:1ms:A");
    REQUIRE(typing.has_value());
    const std::vector<Typing> typed{*typing};
    CHECK(InputEdges(*loaded, presses, {}, {}).error() == "'SW9' is not a switch");
    CHECK(InputEdges(*loaded, {}, knobs, {}).error() == "'J9' is not a paddle controller");
    CHECK(InputEdges(*loaded, {}, {}, typed).error() == "'J9' is not a keyboard");
}

TEST_CASE("run to: stops at each edge and at the end", "[machine]") {
    const auto loaded = Build("dice");
    const auto edges = Edges(*loaded, {{"SW1", 1500_us, 3_ms}, {"SW1", 7_ms, 9_ms}});
    Switch* button = SwitchOf(*loaded, "SW1");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    std::vector<Time> stops;
    std::vector<bool> pressed;
    const auto slice = [&] {
        stops.push_back(sim.now());
        pressed.push_back(button->pressed());
        return true;
    };
    std::size_t next = 0;
    REQUIRE(RunTo(sim, edges, next, 4500_us, slice));
    CHECK(sim.now() == 4500_us);
    CHECK(next == 2);  // the press and its release; the second press is for the next call
    // A millisecond at a time from each stop, an edge's time a stop of its own.
    CHECK(stops == std::vector<Time>{1_ms, 1500_us, 2500_us, 3_ms, 4_ms, 4500_us});
    CHECK(pressed == std::vector<bool>{false, true, true, false, false, false});

    REQUIRE(RunTo(sim, edges, next, 7_ms, slice));  // an edge at the end is applied there
    CHECK(sim.now() == 7_ms);
    CHECK(next == 3);
    CHECK(button->pressed());
    REQUIRE(RunTo(sim, edges, next, 7_ms, slice));  // nothing left to run
    CHECK(stops.back() == 7_ms);
}

TEST_CASE("run to: stopped by its slice", "[machine]") {
    const auto loaded = Build("dice");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    std::size_t next = 0;
    int slices = 0;
    CHECK_FALSE(RunTo(sim, {}, next, 10_ms, [&] { return ++slices < 3; }));
    CHECK(slices == 3);
    CHECK(sim.now() == 3_ms);
}
