#include "parts/keyboard.hpp"

#include <array>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "parts/connector.hpp"
#include "sim/circuit.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

// One keyboard, every signal Pin on its own Net.
struct Bench {
    Circuit circuit;
    Keyboard* keyboard = nullptr;
    std::array<NetId, 7> data{};
    NetId strobe{};

    explicit Bench(Time length = kKeyboardStrobe) {
        keyboard = &circuit.add<Keyboard>("J1", length);
        for (PinIndex bit = 0; bit < 7; ++bit) {
            data[bit] = circuit.addNet("D" + std::to_string(bit));
            circuit.connect(*keyboard, Keyboard::kD0 + bit, data[bit]);
        }
        strobe = circuit.addNet("STROBE");
        circuit.connect(*keyboard, Keyboard::kStrobe, strobe);
        circuit.finalize();
    }

    int code(const Simulator& sim) const {
        int value = 0;
        for (std::size_t bit = 0; bit < 7; ++bit) {
            value |= (sim.value(data[bit]) == One ? 1 : 0) << bit;
        }
        return value;
    }

    // STROBE's changes as (time, value).
    std::vector<std::pair<Time, LogicValue>> edges(const Simulator& sim) const {
        std::vector<std::pair<Time, LogicValue>> result;
        for (const Sample& s : sim.recorder().samples(strobe)) {
            result.emplace_back(s.time, s.value);
        }
        return result;
    }
};

std::expected<Component*, std::string> Create(std::vector<PartParam> params) {
    static Circuit circuit;
    return FindPart("KEYBOARD")->create(circuit, "J1", params);
}

}  // namespace

TEST_CASE("keyboard: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("KEYBOARD");
    REQUIRE(part != nullptr);
    CHECK(part->kind == PartKind::Connector);
    CHECK(part->package == "KK-10");
    const auto created = Create({});
    REQUIRE(created.has_value());
    const auto pins = (*created)->pins();
    REQUIRE(pins.size() == 10);
    for (int bit = 0; bit < 7; ++bit) {
        CHECK(pins[bit].name == "D" + std::to_string(bit));
        CHECK(pins[bit].kind == PinKind::Output);
    }
    CHECK(pins[7].name == "STROBE");
    CHECK(pins[7].kind == PinKind::Output);
    CHECK(pins[8].name == "VCC");
    CHECK(pins[8].kind == PinKind::Power);
    CHECK(pins[9].name == "GND");
    CHECK(pins[9].kind == PinKind::Power);
    CHECK(ConnectorPowersDevice("KEYBOARD"));
}

TEST_CASE("keyboard: strobe parameter", "[parts]") {
    CHECK(Create({{"strobe", "50us"}}).has_value());
    CHECK(Create({{"strobe", "1us"}}).has_value());
    CHECK(Create({{"strobe", "0.5us"}}).error() == "KEYBOARD: strobe must be at least 1us");
    CHECK(Create({{"strobe", "x"}}).error().starts_with("bad duration 'x'"));
    CHECK(Create({{"speed", "1"}}).error() == "KEYBOARD: unknown parameter 'speed'");
}

TEST_CASE("keyboard: KeyboardCode", "[parts]") {
    CHECK(KeyboardCode(U'A') == 0x41);
    CHECK(KeyboardCode(U'a') == 0x41);
    CHECK(KeyboardCode(U'z') == 0x5A);
    CHECK(KeyboardCode(U' ') == 0x20);
    CHECK(KeyboardCode(U'0') == 0x30);
    CHECK(KeyboardCode(U'_') == 0x5F);
    CHECK(KeyboardCode(U'@') == 0x40);
    CHECK(KeyboardCode(U'\r') == 0x0D);
    CHECK(KeyboardCode(U'\n') == 0x0D);
    CHECK(KeyboardCode(U'\b') == 0x08);
    for (const char32_t refused : {U'`', U'{', U'|', U'}', U'~', U'\t', U'\x7f', U'é'}) {
        CHECK_FALSE(KeyboardCode(refused).has_value());
    }
}

TEST_CASE("keyboard: one key, then its strobe", "[parts]") {
    Bench b;
    Simulator sim(b.circuit);
    sim.powerOn(1);
    sim.record(b.strobe);
    CHECK(sim.value(b.strobe) == Zero);
    sim.runUntil(5_us);
    CHECK(b.edges(sim).size() == 1);  // the power-on value only: nothing typed
    REQUIRE(b.keyboard->type(0x48));
    sim.wake(b.keyboard->id());
    const Time t = sim.now();
    sim.runUntil(t + kKeyboardDelay);
    CHECK(b.code(sim) == 0x48);
    sim.runUntil(t + 50_us);
    const auto edges = b.edges(sim);
    REQUIRE(edges.size() == 3);
    CHECK(edges[1] == std::pair{t + 1100_ns, One});
    CHECK(edges[2] == std::pair{t + 1100_ns + kKeyboardStrobe, Zero});
    CHECK(b.code(sim) == 0x48);  // the code stays
}

TEST_CASE("keyboard: keys typed together come in turn", "[parts]") {
    Bench b(20_us);
    Simulator sim(b.circuit);
    sim.powerOn(2);
    sim.record(b.strobe);
    for (const std::uint8_t code : {0x41, 0x42, 0x43}) {
        REQUIRE(b.keyboard->type(code));
    }
    sim.wake(b.keyboard->id());
    std::vector<int> codes;
    const Time period = 20_us + 2100_ns;
    for (int i = 0; i < 3; ++i) {
        const Time rise = 1100_ns + i * period;
        sim.runUntil(rise);
        codes.push_back(b.code(sim));
        // A wake mid-key changes nothing.
        sim.wake(b.keyboard->id());
    }
    sim.runUntil(200_us);
    CHECK(codes == std::vector<int>{0x41, 0x42, 0x43});
    const auto edges = b.edges(sim);
    REQUIRE(edges.size() == 7);
    for (int i = 0; i < 3; ++i) {
        CHECK(edges[1 + 2 * i] == std::pair{1100_ns + i * period, One});
        CHECK(edges[2 + 2 * i] == std::pair{1100_ns + i * period + 20_us, Zero});
    }
}

TEST_CASE("keyboard: at most 32 keys wait", "[parts]") {
    Bench b;
    for (std::size_t i = 0; i < kKeyboardWaiting; ++i) {
        CHECK(b.keyboard->type(0x41));
    }
    CHECK_FALSE(b.keyboard->type(0x41));
}

TEST_CASE("keyboard: power-off drops waiting keys, power-on keeps them", "[parts]") {
    Bench b;
    Simulator sim(b.circuit);
    REQUIRE(b.keyboard->type(0x41));
    sim.powerOn(1);  // a key typed before the power-on applies is kept
    CHECK(sim.value(b.strobe) == Zero);
    sim.wake(b.keyboard->id());
    sim.runUntil(1_us);
    CHECK(b.code(sim) == 0x41);
    sim.runUntil(30_us);

    REQUIRE(b.keyboard->type(0x42));
    b.keyboard->dropWaiting();
    sim.wake(b.keyboard->id());
    sim.runUntil(60_us);
    CHECK(b.code(sim) == 0x41);  // B dropped

    REQUIRE(b.keyboard->type(0x43));
    sim.wake(b.keyboard->id());
    sim.runUntil(90_us);
    CHECK(b.code(sim) == 0x43);
}
