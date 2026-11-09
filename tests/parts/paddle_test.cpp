#include "parts/paddle.hpp"

#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "parts/connector.hpp"
#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kShortest = 30_us;
constexpr Time kLongest = 9930_us;

// One controller, TRIG_N from a Stimulus, OUT on its own Net.
struct Bench {
    Circuit circuit;
    Paddle* paddle = nullptr;
    NetId out{};

    explicit Bench(std::vector<Stimulus::Step> trigger, LogicValue initial = One) {
        paddle = &circuit.add<Paddle>("J2", kShortest, kLongest);
        auto& source = circuit.add<Stimulus>("STRIG", initial, std::move(trigger));
        const NetId trig = circuit.addNet("TRIG_N");
        circuit.connect(source, Stimulus::kOut, trig);
        circuit.connect(*paddle, Paddle::kTrigN, trig);
        out = circuit.addNet("OUT");
        circuit.connect(*paddle, Paddle::kOut, out);
        circuit.finalize();
    }
};

std::expected<Component*, std::string> Create(std::vector<PartParam> params) {
    static Circuit circuit;
    return FindPart("PADDLE")->create(circuit, "J2", params);
}

}  // namespace

TEST_CASE("paddle: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("PADDLE");
    REQUIRE(part != nullptr);
    CHECK(part->kind == PartKind::Connector);
    CHECK(part->package == "KK-4");
    const auto created = Create({{"min", "30us"}, {"max", "9.93ms"}});
    REQUIRE(created.has_value());
    const auto pins = (*created)->pins();
    REQUIRE(pins.size() == 4);
    CHECK(pins[0].name == "TRIG_N");
    CHECK(pins[0].kind == PinKind::Input);
    CHECK(pins[1].name == "OUT");
    CHECK(pins[1].kind == PinKind::Output);
    CHECK(pins[2].name == "VCC");
    CHECK(pins[2].kind == PinKind::Power);
    CHECK(pins[3].name == "GND");
    CHECK(pins[3].kind == PinKind::Power);
    CHECK(ConnectorPowersDevice("PADDLE"));
    CHECK_FALSE(ConnectorPowersDevice("POWER"));
    CHECK_FALSE(ConnectorPowersDevice("VIDEO"));
    CHECK_FALSE(ConnectorVolts("PADDLE", "VCC").has_value());
}

TEST_CASE("paddle: parameters", "[parts]") {
    const auto error = [](std::vector<PartParam> params) {
        const auto created = Create(std::move(params));
        REQUIRE_FALSE(created.has_value());
        return created.error();
    };
    CHECK(error({{"min", "30us"}}) == "PADDLE needs min=<duration> and max=<duration>");
    CHECK(error({{"max", "9ms"}}) == "PADDLE needs min=<duration> and max=<duration>");
    CHECK(error({{"min", "30us"}, {"max", "9ms"}, {"freq", "1MHz"}}) ==
          "PADDLE: unknown parameter 'freq'");
    CHECK(error({{"min", "30"}, {"max", "9ms"}}).starts_with("bad duration '30'"));
    CHECK(error({{"min", "500ns"}, {"max", "9ms"}}) == "PADDLE: min must be at least 1us");
    CHECK(error({{"min", "9ms"}, {"max", "9ms"}}) == "PADDLE: max must be longer than min");
    CHECK(error({{"min", "30us"}, {"max", "9ms"}, {"reversed", "maybe"}}) ==
          "PADDLE: reversed must be yes or no");
    CHECK(Create({{"min", "30us"}, {"max", "9ms"}, {"reversed", "no"}}).has_value());
}

TEST_CASE("paddle: reversed, the longest pulse at the left stop", "[parts]") {
    const auto created = Create({{"min", "30us"}, {"max", "9930us"}, {"reversed", "yes"}});
    REQUIRE(created.has_value());
    auto* paddle = dynamic_cast<Paddle*>(*created);
    REQUIRE(paddle != nullptr);
    paddle->setKnob(0.0);
    CHECK(paddle->pulseLength() == kLongest);
    paddle->setKnob(1.0);
    CHECK(paddle->pulseLength() == kShortest);
    paddle->setKnob(0.5);
    CHECK(paddle->pulseLength() == kLongest - (kLongest - kShortest) / 2);
}

TEST_CASE("paddle: idle low, one pulse per fall, as long as the knob says", "[parts]") {
    struct Case {
        double knob;
        Time length;
    };
    for (const Case c : {Case{0.0, kShortest}, Case{1.0, kLongest},
                         Case{0.5, kShortest + (kLongest - kShortest) / 2}}) {
        INFO("knob " << c.knob);
        Bench b({{1_ms, Zero}, {1_ms + 10_us, One}});
        b.paddle->setKnob(c.knob);
        CHECK(b.paddle->pulseLength() == c.length);
        Simulator sim(b.circuit);
        sim.powerOn(1);
        sim.runUntil(1_ms + kPaddleDelay - 1);
        CHECK(sim.value(b.out) == Zero);
        sim.runUntil(1_ms + kPaddleDelay);
        CHECK(sim.value(b.out) == One);
        sim.runUntil(1_ms + c.length + kPaddleDelay - 1);
        CHECK(sim.value(b.out) == One);
        sim.runUntil(1_ms + c.length + kPaddleDelay);
        CHECK(sim.value(b.out) == Zero);
        sim.runUntil(20_ms);
        CHECK(sim.value(b.out) == Zero);
    }
}

TEST_CASE("paddle: a fall during a pulse is ignored", "[parts]") {
    Bench b({{1_ms, Zero}, {1_ms + 10_us, One}, {1_ms + 20_us, Zero}, {1_ms + 25_us, One}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(1_ms + kShortest + kPaddleDelay);
    CHECK(sim.value(b.out) == Zero);  // ended on the first fall's time
    sim.runUntil(2_ms);
    CHECK(sim.value(b.out) == Zero);
}

TEST_CASE("paddle: rises and X do nothing", "[parts]") {
    Bench b({{1_ms, One}, {2_ms, X}, {3_ms, Zero}}, Zero);
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(3_ms);
    CHECK(sim.value(b.out) == Zero);
    sim.runUntil(3_ms + kPaddleDelay + 1_us);
    CHECK(sim.value(b.out) == Zero);  // X → 0 is no fall
}

TEST_CASE("paddle: a turn during a pulse waits for the next", "[parts]") {
    Bench b({{1_ms, Zero}, {1_ms + 10_us, One}, {20_ms, Zero}});
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.runUntil(1_ms + 10_us);
    b.paddle->setKnob(1.0);
    sim.runUntil(1_ms + kShortest + kPaddleDelay);
    CHECK(sim.value(b.out) == Zero);
    sim.runUntil(20_ms + kLongest + kPaddleDelay - 1);
    CHECK(sim.value(b.out) == One);
    sim.runUntil(20_ms + kLongest + kPaddleDelay);
    CHECK(sim.value(b.out) == Zero);
}

TEST_CASE("paddle: the knob is clamped", "[parts]") {
    Paddle paddle("J2", kShortest, kLongest);
    CHECK(paddle.knob() == 0.0);
    paddle.setKnob(1.5);
    CHECK(paddle.knob() == 1.0);
    paddle.setKnob(-0.2);
    CHECK(paddle.knob() == 0.0);
}
