#include "parts/shift_register_2518.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "parts/delays.hpp"
#include "sim/circuit.hpp"
#include "sim/primitives.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;
using Steps = std::vector<Stimulus::Step>;

constexpr Time kPeriod = 1500_ns;  // one clock, as on the 090 Board: a character

// `count` clocks from `start`: low for 1 µs, rising at start + (i + 1) × 1.5 µs.
Steps Clock(Time start, std::size_t count) {
    Steps steps;
    for (std::size_t i = 0; i < count; ++i) {
        const Time t = start + static_cast<Time>(i) * kPeriod;
        steps.push_back({t + 500_ns, Zero});
        steps.push_back({t + kPeriod, One});
    }
    return steps;
}

// One 2518: CLK, REC and IN1-IN6 each driven by a Stimulus (IN all low unless given).
struct Bench {
    Circuit circuit;
    ShiftRegister2518* chip = nullptr;
    std::array<NetId, 6> out{};

    Bench(const Steps& clk, LogicValue rec, const Steps& recSteps,
          const std::array<Steps, 6>& in = {}) {
        chip = &circuit.add<ShiftRegister2518>("U1");
        const auto drive = [&](PinIndex pin, LogicValue initial, const Steps& steps) {
            const std::string name(chip->pins()[pin].name);
            const NetId net = circuit.addNet(name);
            auto& source = circuit.add<Stimulus>("S" + name, initial, steps);
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(*chip, pin, net);
        };
        drive(ShiftRegister2518::kClk, One, clk);
        drive(ShiftRegister2518::kRec, rec, recSteps);
        for (std::size_t i = 0; i < 6; ++i) {
            drive(ShiftRegister2518::kIn[i], Zero, in[i]);
            out[i] = circuit.addNet("OUT" + std::to_string(i + 1));
            circuit.connect(*chip, ShiftRegister2518::kOut[i], out[i]);
        }
        circuit.finalize();
    }

    // OUT1-OUT6 as a word; -1 if one is not 0 or 1.
    int word(const Simulator& sim) const {
        int value = 0;
        for (std::size_t i = 0; i < 6; ++i) {
            const LogicValue v = sim.value(out[i]);
            if (v != Zero && v != One) {
                return -1;
            }
            value |= (v == One ? 1 : 0) << i;
        }
        return value;
    }
};

}  // namespace

TEST_CASE("2518: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("2518");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-16");
    CHECK(part->kind == PartKind::Chip);

    ShiftRegister2518 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 16);
    const std::array<std::string_view, 16> names{"IN4",  "IN5",  "IN6",  "REC",  "VGG",  "CLK",
                                                 "OUT6", "OUT5", "OUT4", "OUT3", "OUT2", "OUT1",
                                                 "IN1",  "IN2",  "IN3",  "VCC"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        INFO(i);
        CHECK(pins[i].name == names[i]);
    }
    for (std::size_t i = 0; i < 6; ++i) {
        CHECK(pins[ShiftRegister2518::kIn[i]].name == "IN" + std::to_string(i + 1));
        CHECK(pins[ShiftRegister2518::kOut[i]].name == "OUT" + std::to_string(i + 1));
        CHECK(pins[ShiftRegister2518::kOut[i]].kind == PinKind::Output);
    }
    CHECK(pins[ShiftRegister2518::kClk].risingEdgeOnly);
    CHECK(pins[4].kind == PinKind::NoConnect);
    CHECK(pins[15].kind == PinKind::Power);
}

TEST_CASE("2518: recirculates, round in 32 clocks", "[parts]") {
    Bench bench(Clock(10_us, 32), One, {});
    Simulator sim(bench.circuit);
    sim.powerOn(3);
    std::array<std::uint8_t, 32> start{};
    for (std::size_t i = 0; i < 32; ++i) {
        start[i] = bench.chip->word(i);
    }
    std::set<int> seen;
    for (std::size_t i = 0; i < 32; ++i) {
        // 400 ns after clock i rose: word i + 1 at the outputs.
        sim.runUntil(10_us + static_cast<Time>(i + 1) * kPeriod + 400_ns);
        INFO(i);
        CHECK(bench.word(sim) == start[(i + 1) % 32]);
        seen.insert(bench.word(sim));
    }
    CHECK(seen.size() > 8);
    for (std::size_t i = 0; i < 32; ++i) {
        CHECK(bench.chip->word(i) == start[i]);
    }
}

TEST_CASE("2518: REC low writes each register's input", "[parts]") {
    // Clocks 0-31 with REC low: word k gets 0b100101 + k (6 bits); then 32 recirculating.
    std::array<Steps, 6> in;
    for (std::size_t k = 0; k < 32; ++k) {
        const unsigned value = (0b100101u + k) & 63u;
        const Time t = 10_us + static_cast<Time>(k) * kPeriod;
        for (std::size_t i = 0; i < 6; ++i) {
            in[i].push_back({t + 200_ns, (value >> i) & 1 ? One : Zero});
        }
    }
    Bench bench(Clock(10_us, 64), Zero, {{10_us + 32 * kPeriod + 200_ns, One}}, in);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(10_us + 64 * kPeriod + 400_ns);
    // 64 clocks: the head is back on word 0, which took the input of clock 0.
    for (std::size_t k = 0; k < 32; ++k) {
        INFO(k);
        CHECK(bench.chip->word(k) == ((0b100101u + k) & 63u));
    }
    CHECK(bench.word(sim) == 0b100101);
}

TEST_CASE("2518: outputs change 300 ns after CLK rises", "[parts]") {
    Bench bench(Clock(10_us, 1), One, {});
    Simulator sim(bench.circuit);
    for (std::uint64_t seed = 1; seed <= 16; ++seed) {
        sim.powerOn(seed);
        const int next = bench.chip->word(1);
        const int now = bench.chip->word(0);
        if (next == now) {
            continue;
        }
        const Time rise = 10_us + kPeriod;
        sim.runUntil(rise + delays::k2518ClockToOutput - 1);
        CHECK(bench.word(sim) == now);
        sim.runUntil(rise + delays::k2518ClockToOutput);
        CHECK(bench.word(sim) == next);
        return;
    }
    FAIL("no seed with two different first words");
}

TEST_CASE("2518: random contents at power-on", "[parts]") {
    Bench bench({}, One, {});
    Simulator sim(bench.circuit);
    const auto words = [&] {
        std::array<std::uint8_t, 32> w{};
        for (std::size_t i = 0; i < 32; ++i) {
            w[i] = bench.chip->word(i);
        }
        return w;
    };
    sim.powerOn(1);
    const auto one = words();
    CHECK(std::set<std::uint8_t>(one.begin(), one.end()).size() > 8);
    sim.powerOn(1);
    CHECK(words() == one);
    sim.powerOn(2);
    CHECK(words() != one);
}
