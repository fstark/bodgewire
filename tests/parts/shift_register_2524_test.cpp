#include "parts/shift_register_2524.hpp"

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
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

constexpr Time kPair = 1500_ns;  // one clock pair, as on the 090 Board: a character

// Clock pairs from `start`, one every 1.5 µs: PHI1 low 250-500 ns into the pair, PHI2 low
// 750-1000 ns (dots 11 and 13 of a 090 character). `phi1`/`phi2` false leave that clock out.
Steps Clock(Time start, std::size_t pairs, Time low, bool on = true) {
    Steps steps;
    if (!on) {
        return steps;
    }
    for (std::size_t i = 0; i < pairs; ++i) {
        const Time t = start + static_cast<Time>(i) * kPair;
        steps.push_back({t + low, Zero});
        steps.push_back({t + low + 250_ns, One});
    }
    return steps;
}

struct Signals {
    Steps phi1, phi2, write, in;
    LogicValue read = One;
};

// One 2524, every input driven by a Stimulus; clocks idle high, WRITE and IN low unless given.
struct Bench {
    Circuit circuit;
    ShiftRegister2524* chip = nullptr;
    NetId out{};

    explicit Bench(const Signals& s) {
        chip = &circuit.add<ShiftRegister2524>("U1");
        const auto drive = [&](PinIndex pin, LogicValue initial, const Steps& steps) {
            const std::string name(chip->pins()[pin].name);
            const NetId net = circuit.addNet(name);
            auto& source = circuit.add<Stimulus>("S" + name, initial, steps);
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(*chip, pin, net);
        };
        drive(ShiftRegister2524::kPhi1, One, s.phi1);
        drive(ShiftRegister2524::kPhi2, One, s.phi2);
        drive(ShiftRegister2524::kRead, s.read, {});
        drive(ShiftRegister2524::kWrite, Zero, s.write);
        drive(ShiftRegister2524::kIn, Zero, s.in);
        out = circuit.addNet("OUT");
        circuit.connect(*chip, ShiftRegister2524::kOut, out);
        circuit.finalize();
    }

    std::bitset<512> bits() const {
        std::bitset<512> b;
        for (std::size_t i = 0; i < 512; ++i) {
            b[i] = chip->bit(i);
        }
        return b;
    }
};

// The ring as it stands, read from the output: bits() after `shifts` more shifts.
std::bitset<512> Rotated(const std::bitset<512>& b, std::size_t shifts) {
    std::bitset<512> r;
    for (std::size_t i = 0; i < 512; ++i) {
        r[i] = b[(i + shifts) % 512];
    }
    return r;
}

}  // namespace

TEST_CASE("2524: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("2524");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-8");
    CHECK(part->kind == PartKind::Chip);

    ShiftRegister2524 chip("U1");
    const auto pins = chip.pins();
    REQUIRE(pins.size() == 8);
    const std::array<std::string_view, 8> names{"PHI2",  "OUT", "READ", "VDD",
                                                "WRITE", "IN",  "PHI1", "VCC"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        INFO(i);
        CHECK(pins[i].name == names[i]);
    }
    CHECK(pins[ShiftRegister2524::kPhi1].risingEdgeOnly);
    CHECK(pins[ShiftRegister2524::kPhi2].risingEdgeOnly);
    CHECK(pins[ShiftRegister2524::kOut].kind == PinKind::Output);
    CHECK(pins[3].kind == PinKind::NoConnect);
    CHECK(pins[7].kind == PinKind::Power);
    CHECK(pins[ShiftRegister2524::kIn].family == Family::Mos);
}

TEST_CASE("2524: recirculates, one bit out per clock pair", "[parts]") {
    Bench bench({Clock(10_us, 512, 250_ns), Clock(10_us, 512, 750_ns), {}, {}});
    Simulator sim(bench.circuit);
    sim.powerOn(7);
    const std::bitset<512> start = bench.bits();
    std::set<bool> seen;
    for (std::size_t i = 0; i < 512; ++i) {
        // After pair i's PHI1 (and its 100 ns), OUT shows bit i + 1 of the start.
        sim.runUntil(10_us + static_cast<Time>(i) * kPair + 700_ns);
        INFO(i);
        REQUIRE(sim.value(bench.out) == (start[(i + 1) % 512] ? One : Zero));
        seen.insert(start[(i + 1) % 512]);
    }
    CHECK(seen.size() == 2);
    CHECK(bench.bits() == start);  // 512 shifts: back where it started
}

TEST_CASE("2524: a write replaces the bit at the output, seen when it comes round", "[parts]") {
    // Pair 3 writes: WRITE high around its PHI2, IN the opposite of the bit at the output.
    const Time pair3 = 10_us + 3 * kPair;
    for (const bool value : {false, true}) {
        INFO(value);
        Bench bench({Clock(10_us, 512 + 4, 250_ns),
                     Clock(10_us, 512 + 4, 750_ns),
                     {{pair3 + 600_ns, One}, {pair3 + 1100_ns, Zero}},
                     {{pair3 + 600_ns, value ? One : Zero}, {pair3 + 1100_ns, Zero}}});
        Simulator sim(bench.circuit);
        sim.powerOn(3);
        const std::bitset<512> start = bench.bits();
        sim.runUntil(pair3 + 700_ns);
        const LogicValue before = sim.value(bench.out);
        CHECK(before == (start[4] ? One : Zero));
        sim.runUntil(pair3 + 1400_ns);
        CHECK(sim.value(bench.out) == before);  // the write does not show at OUT
        std::bitset<512> expected = start;
        expected[4] = value;
        CHECK(bench.bits() == Rotated(expected, 4));
        // 512 pairs after pair 3, the written bit is at the output again.
        sim.runUntil(pair3 + 512 * kPair + 700_ns);
        CHECK(sim.value(bench.out) == (value ? One : Zero));
    }
}

TEST_CASE("2524: READ low gives 0", "[parts]") {
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        Signals s{Clock(10_us, 16, 250_ns), Clock(10_us, 16, 750_ns), {}, {}};
        s.read = Zero;
        Bench bench(s);
        Simulator sim(bench.circuit);
        sim.powerOn(seed);
        for (std::size_t i = 0; i < 16; ++i) {
            sim.runUntil(10_us + static_cast<Time>(i) * kPair + 700_ns);
            CHECK(sim.value(bench.out) == Zero);
        }
    }
}

TEST_CASE("2524: OUT changes 100 ns after PHI1 rises", "[parts]") {
    Bench bench({Clock(10_us, 64, 250_ns), Clock(10_us, 64, 750_ns), {}, {}});
    Simulator sim(bench.circuit);
    sim.powerOn(5);
    sim.record(bench.out);
    const std::bitset<512> start = bench.bits();
    sim.runUntil(10_us + 64 * kPair);
    int changes = 0;
    for (const Sample& change : sim.recorder().samples(bench.out)) {
        if (change.time < 10_us) {
            continue;  // the initial sample
        }
        const Time intoPair = (change.time - 10_us) % kPair;
        CHECK(intoPair == 500_ns + delays::k2524ClockToOutput);
        ++changes;
    }
    int expected = 0;
    for (std::size_t i = 0; i < 64; ++i) {
        expected += start[i] != start[i + 1] ? 1 : 0;
    }
    CHECK(changes == expected);
}

TEST_CASE("2524: PHI2 alone never moves the ring; PHI1 alone shifts without writing", "[parts]") {
    {
        Bench bench({{}, Clock(10_us, 32, 750_ns), {{1_us, One}}, {}});
        Simulator sim(bench.circuit);
        sim.powerOn(9);
        const std::bitset<512> start = bench.bits();
        const LogicValue out = sim.value(bench.out);
        sim.runUntil(10_us + 32 * kPair);
        CHECK(sim.value(bench.out) == out);
        // Only the bit at the output was written (with IN = 0), 32 times.
        std::bitset<512> expected = start;
        expected[0] = false;
        CHECK(bench.bits() == expected);
    }
    {
        Bench bench({Clock(10_us, 32, 250_ns), {}, {{1_us, One}}, {{1_us, One}}});
        Simulator sim(bench.circuit);
        sim.powerOn(9);
        const std::bitset<512> start = bench.bits();
        sim.runUntil(10_us + 32 * kPair);
        CHECK(bench.bits() == Rotated(start, 32));
    }
}

TEST_CASE("2524: X on WRITE or IN is read as a random bit", "[parts]") {
    const Time pair0 = 10_us;
    for (const bool xOnWrite : {false, true}) {
        INFO(xOnWrite);
        std::set<bool> seen;
        for (std::uint64_t seed = 1; seed <= 32; ++seed) {
            Signals s{Clock(pair0, 1, 250_ns), Clock(pair0, 1, 750_ns), {}, {}};
            // xOnWrite: WRITE X, IN the opposite of the bit; else WRITE high, IN X.
            Bench probe(s);
            Simulator first(probe.circuit);
            first.powerOn(seed);
            const bool bit = probe.chip->bit(1);
            s.write = {{pair0 + 600_ns, xOnWrite ? X : One}, {pair0 + 1100_ns, Zero}};
            s.in = {{pair0 + 600_ns, xOnWrite ? (bit ? Zero : One) : X}, {pair0 + 1100_ns, Zero}};
            Bench bench(s);
            Simulator sim(bench.circuit);
            sim.powerOn(seed);
            sim.runUntil(pair0 + kPair);
            seen.insert(bench.chip->bit(0) == bit);
        }
        CHECK(seen.size() == 2);
    }
}

TEST_CASE("2524: random contents at power-on", "[parts]") {
    Bench bench({});
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    const std::bitset<512> one = bench.bits();
    CHECK(one.count() > 192);
    CHECK(one.count() < 320);
    sim.powerOn(1);
    CHECK(bench.bits() == one);
    sim.powerOn(2);
    CHECK(bench.bits() != one);
}

TEST_CASE("2524: clocks stopped 9 ms keep the bits; after 11 ms they fade", "[parts]") {
    // One clock pair, a pause, then another pair: PHI2 rose last before the pause.
    for (const Time pause : {9_ms, 11_ms}) {
        INFO(pause);
        Steps phi1 = Clock(10_us, 1, 250_ns);
        Steps phi2 = Clock(10_us, 1, 750_ns);
        const Steps more1 = Clock(10_us + kPair + pause, 1, 250_ns);
        const Steps more2 = Clock(10_us + kPair + pause, 1, 750_ns);
        phi1.insert(phi1.end(), more1.begin(), more1.end());
        phi2.insert(phi2.end(), more2.begin(), more2.end());
        Bench bench({phi1, phi2, {}, {}});
        Simulator sim(bench.circuit);
        sim.powerOn(4);
        const std::bitset<512> start = bench.bits();
        sim.runUntil(10_us + 2 * kPair + pause);
        if (pause < delays::k2524Hold) {
            CHECK(bench.bits() == Rotated(start, 2));
        } else {
            CHECK(bench.bits().all());  // every bit 1
        }
    }
}

TEST_CASE("2524: a fade takes the level of the last clock phase", "[parts]") {
    // A pair, then PHI1 alone, then the pause: PHI1 rose last, so the bits fade to 0.
    Steps phi1 = Clock(10_us, 2, 250_ns);
    const Steps again = Clock(10_us + 2 * kPair + 11_ms, 1, 250_ns);
    phi1.insert(phi1.end(), again.begin(), again.end());
    Bench bench({phi1, Clock(10_us, 1, 750_ns), {}, {}});
    Simulator sim(bench.circuit);
    sim.powerOn(5);
    REQUIRE(bench.bits().any());
    sim.runUntil(10_us + 3 * kPair + 11_ms);
    CHECK(bench.bits().none());
}

TEST_CASE("2524: with one clock stopped the bits fade once and stay", "[parts]") {
    // PHI1 runs for 25 ms, PHI2 stops after the first pair: faded to 0 after 10 ms, still 0
    // later, and OUT shows it.
    constexpr std::size_t kPairs = 16'666;
    Bench bench({Clock(10_us, kPairs, 250_ns), Clock(10_us, 1, 750_ns), {}, {}});
    Simulator sim(bench.circuit);
    sim.powerOn(6);
    sim.runUntil(1_ms);
    REQUIRE(bench.bits().any());
    sim.runUntil(9_ms);
    CHECK(bench.bits().any());
    sim.runUntil(11_ms);
    CHECK(bench.bits().none());
    sim.runUntil(24_ms);
    CHECK(bench.bits().none());
    CHECK(sim.value(bench.out) == Zero);
}

TEST_CASE("2524: written after a fade, the bits are kept", "[parts]") {
    // Clocks stop 11 ms (PHI2 last: faded to 1), then run 1024 pairs: the first 512 write 0s,
    // the next 512 recirculate them.
    const Time restart = 10_us + kPair + 11_ms;
    Steps phi1 = Clock(10_us, 1, 250_ns), phi2 = Clock(10_us, 1, 750_ns);
    const Steps run1 = Clock(restart, 1024, 250_ns), run2 = Clock(restart, 1024, 750_ns);
    phi1.insert(phi1.end(), run1.begin(), run1.end());
    phi2.insert(phi2.end(), run2.begin(), run2.end());
    Bench bench({phi1, phi2, {{restart, One}, {restart + 512 * kPair, Zero}}, {}});
    Simulator sim(bench.circuit);
    sim.powerOn(7);
    sim.runUntil(restart + 600_ns);
    CHECK(bench.bits().all());
    sim.runUntil(restart + 1024 * kPair + 1_us);
    CHECK(bench.bits().none());
}
