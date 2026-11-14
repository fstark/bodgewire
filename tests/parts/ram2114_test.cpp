#include "parts/ram2114.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <ranges>
#include <string>
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

struct Input {
    LogicValue initial;
    Steps steps;
};

// One 2114. Every input driven by a Stimulus (listed, or 0); each data Net driven by a MOS
// Stimulus that is Z unless listed, standing for the other chip on the bus.
struct Bench {
    Circuit circuit;
    Ram2114* ram = nullptr;
    std::array<NetId, 4> data{};

    Bench(const std::map<std::string, Input>& inputs, const std::array<Input, 4>& bus) {
        ram = &circuit.add<Ram2114>("U1");
        const auto pins = ram->pins();
        for (PinIndex pin = 0; pin < pins.size(); ++pin) {
            if (pins[pin].kind != PinKind::Input) {
                continue;
            }
            const std::string name = pins[pin].name;
            Input input{Zero, {}};
            if (const auto it = inputs.find(name); it != inputs.end()) {
                input = it->second;
            }
            const NetId net = circuit.addNet(name);
            auto& source = circuit.add<Stimulus>("S" + name, input.initial, input.steps);
            circuit.connect(source, Stimulus::kOut, net);
            circuit.connect(*ram, pin, net);
        }
        for (std::size_t b = 0; b < 4; ++b) {
            data[b] = circuit.addNet("IO" + std::to_string(b + 1));
            auto& source = circuit.add<Stimulus>("D" + std::to_string(b), bus[b].initial,
                                                 bus[b].steps, Family::Mos);
            circuit.connect(source, Stimulus::kOut, data[b]);
            circuit.connect(*ram, Ram2114::kData[b], data[b]);
        }
        circuit.finalize();
    }

    // The data Nets as a number; -1 if any is floating or not 0/1.
    int word(const Simulator& sim) const {
        int value = 0;
        for (std::size_t b = 0; b < 4; ++b) {
            const LogicValue v = sim.value(data[b]);
            if (sim.isFloating(data[b]) || (v != Zero && v != One)) {
                return -1;
            }
            value |= (v == One ? 1 : 0) << b;
        }
        return value;
    }

    bool floating(const Simulator& sim) const {
        for (const NetId net : data) {
            if (!sim.isFloating(net)) {
                return false;
            }
        }
        return true;
    }
};

// A bus Driver showing `value` from `from` to `to`, Z otherwise; one Input per data bit.
std::array<Input, 4> BusWord(std::vector<std::pair<std::pair<Time, Time>, int>> windows) {
    std::array<Input, 4> bus;
    for (std::size_t b = 0; b < 4; ++b) {
        bus[b].initial = Z;
        for (const auto& [span, value] : windows) {
            auto& steps = bus[b].steps;
            if (!steps.empty() && steps.back().time == span.first) {
                steps.pop_back();  // windows back to back: no release between them
            }
            steps.push_back({span.first, (value >> b) & 1 ? One : Zero});
            steps.push_back({span.second, Z});
        }
    }
    return bus;
}

}  // namespace

TEST_CASE("2114: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("2114");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-18");
    CHECK(part->kind == PartKind::Chip);

    Ram2114 ram("U1");
    const auto pins = ram.pins();
    REQUIRE(pins.size() == 18);
    const std::array<std::string_view, 18> names{"A6",  "A5",   "A4",  "A3",   "A0",  "A1",
                                                 "A2",  "CS_N", "GND", "WE_N", "IO4", "IO3",
                                                 "IO2", "IO1",  "A9",  "A8",   "A7",  "VCC"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        INFO(i);
        CHECK(pins[i].name == names[i]);
        if (names[i] == "GND" || names[i] == "VCC") {
            CHECK(pins[i].kind == PinKind::Power);
        } else if (names[i].starts_with("IO")) {
            CHECK(pins[i].kind == PinKind::Bidirectional);
            CHECK(pins[i].family == Family::Mos);
        } else {
            CHECK(pins[i].kind == PinKind::Input);
            CHECK(pins[i].family == Family::Mos);
        }
    }
    for (std::size_t bit = 0; bit < 10; ++bit) {
        CHECK(pins[Ram2114::kAddress[bit]].name == "A" + std::to_string(bit));
    }
    for (std::size_t bit = 0; bit < 4; ++bit) {
        CHECK(pins[Ram2114::kData[bit]].name == "IO" + std::to_string(bit + 1));
    }
}

TEST_CASE("2114: writes, reads and their delays", "[parts]") {
    // Address 5, then 6 from 2.5 us, 5 again from 4.5 us. Writes 1010 at 5 (1-2 us) and 0101 at
    // 6 (3-4 us). CS_N high from 6 us to 7 us.
    Bench bench(
        {{"A0", {One, {{2500_ns, Zero}, {4500_ns, One}}}},
         {"A1", {Zero, {{2500_ns, One}, {4500_ns, Zero}}}},
         {"A2", {One, {}}},
         {"WE_N", {One, {{1000_ns, Zero}, {2000_ns, One}, {3000_ns, Zero}, {4000_ns, One}}}},
         {"CS_N", {Zero, {{6000_ns, One}, {7000_ns, Zero}}}}},
        BusWord({{{900_ns, 2100_ns}, 0b1010}, {{3150_ns, 4100_ns}, 0b0101}}));
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(2010_ns);
    CHECK(bench.ram->word(5) == 0b1010);
    // WE_N back high at 2 us: driven k2114Select later (the bus Driver let go at 2.1 us).
    sim.runUntil(2000_ns + delays::k2114Select - 1);
    CHECK(bench.floating(sim));
    sim.runUntil(2000_ns + delays::k2114Select);
    CHECK(bench.word(sim) == 0b1010);
    // A write turns the outputs off k2114Off after WE_N falls.
    sim.runUntil(3000_ns + delays::k2114Off - 1);
    CHECK_FALSE(bench.floating(sim));
    sim.runUntil(3000_ns + delays::k2114Off);
    CHECK(bench.floating(sim));
    sim.runUntil(4000_ns + delays::k2114Select);
    CHECK(bench.ram->word(6) == 0b0101);
    CHECK(bench.word(sim) == 0b0101);
    // Address 6 → 5 at 4.5 us: k2114Address later.
    sim.runUntil(4500_ns + delays::k2114Address - 1);
    CHECK(bench.word(sim) == 0b0101);
    sim.runUntil(4500_ns + delays::k2114Address);
    CHECK(bench.word(sim) == 0b1010);
    // Deselected: off k2114Off later; selected again: driven k2114Select later.
    sim.runUntil(6000_ns + delays::k2114Off - 1);
    CHECK_FALSE(bench.floating(sim));
    sim.runUntil(6000_ns + delays::k2114Off);
    CHECK(bench.floating(sim));
    sim.runUntil(7000_ns + delays::k2114Select - 1);
    CHECK(bench.floating(sim));
    sim.runUntil(7000_ns + delays::k2114Select);
    CHECK(bench.word(sim) == 0b1010);
}

TEST_CASE("2114: a write keeps the data held at its end", "[parts]") {
    // Data 1111 then 0110 during the write at address 1.
    Bench bench({{"A0", {One, {}}}, {"WE_N", {One, {{1000_ns, Zero}, {2000_ns, One}}}}},
                BusWord({{{1000_ns, 1500_ns}, 0b1111}, {{1500_ns, 2050_ns}, 0b0110}}));
    Simulator sim(bench.circuit);
    sim.powerOn(2);
    sim.runUntil(3000_ns);
    CHECK(bench.ram->word(1) == 0b0110);
    CHECK(bench.word(sim) == 0b0110);
}

TEST_CASE("2114: the address held at the end of a write is the one written", "[parts]") {
    // The address goes 1 → 2 at the very timestamp WE_N rises: 1 is written, 2 is not.
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        Bench bench({{"A0", {One, {{2000_ns, Zero}}}},
                     {"A1", {Zero, {{2000_ns, One}}}},
                     {"WE_N", {One, {{1000_ns, Zero}, {2000_ns, One}}}}},
                    BusWord({{{1000_ns, 2050_ns}, 0b0011}}));
        Simulator sim(bench.circuit);
        sim.powerOn(seed);
        const std::uint8_t before = bench.ram->word(2);
        sim.runUntil(3000_ns);
        CHECK(bench.ram->word(1) == 0b0011);
        CHECK(bench.ram->word(2) == before);
    }
}

TEST_CASE("2114: X on an address bit picks a word, never X", "[parts]") {
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        INFO(seed);
        // A9 X throughout: the write goes to 0 or 512; the reads never show X.
        Bench bench({{"A9", {X, {}}}, {"WE_N", {One, {{1000_ns, Zero}, {2000_ns, One}}}}},
                    BusWord({{{1000_ns, 2050_ns}, 0b1001}}));
        Simulator sim(bench.circuit);
        sim.powerOn(seed);
        sim.runUntil(900_ns);
        CHECK(bench.word(sim) >= 0);
        sim.runUntil(3000_ns);
        CHECK(bench.word(sim) >= 0);
        CHECK((bench.ram->word(0) == 0b1001 || bench.ram->word(512) == 0b1001));
    }
}

TEST_CASE("2114: random contents at power-on, set by the seed", "[parts]") {
    const auto contents = [](std::uint64_t seed) {
        Bench bench({{"CS_N", {One, {}}}}, BusWord({}));
        Simulator sim(bench.circuit);
        sim.powerOn(seed);
        std::vector<std::uint8_t> words;
        for (std::size_t a = 0; a < 1024; ++a) {
            words.push_back(bench.ram->word(a));
        }
        return words;
    };
    const auto one = contents(1);
    CHECK(contents(1) == one);
    CHECK(contents(2) != one);
    CHECK(std::ranges::any_of(one, [](std::uint8_t w) { return w != 0; }));
    CHECK(std::ranges::all_of(one, [](std::uint8_t w) { return w < 16; }));
}

TEST_CASE("2114: a stuck cell holds its value through writes and power-on", "[parts]") {
    // Writes 0000 at 3 (1-2 us) and at 2 (3-4 us), then reads 3 from 4.5 us.
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        INFO(seed);
        Bench bench(
            {{"A0", {One, {{2500_ns, Zero}, {4500_ns, One}}}},
             {"A1", {One, {}}},
             {"WE_N", {One, {{1000_ns, Zero}, {2000_ns, One}, {3000_ns, Zero}, {4000_ns, One}}}}},
            BusWord({{{900_ns, 2100_ns}, 0}, {{3150_ns, 4100_ns}, 0}}));
        CHECK(bench.ram->wordCount() == 1024);
        CHECK(bench.ram->dataBit(Ram2114::kData[2]) == 2u);
        CHECK_FALSE(bench.ram->dataBit(Ram2114::kAddress[0]).has_value());
        bench.ram->stick(3, 2, true);
        CHECK((bench.ram->word(3) & 0b0100) != 0);
        Simulator sim(bench.circuit);
        sim.powerOn(seed);
        CHECK((bench.ram->word(3) & 0b0100) != 0);
        sim.runUntil(2010_ns);
        CHECK(bench.ram->word(3) == 0b0100);
        sim.runUntil(4010_ns);
        CHECK(bench.ram->word(2) == 0);
        sim.runUntil(4500_ns + delays::k2114Address);
        CHECK(bench.word(sim) == 0b0100);
    }
}

TEST_CASE("2114: a stuck column sticks every word", "[parts]") {
    // Writes 1111 at 0 (1-2 us) and at 1 (3-4 us).
    Bench bench(
        {{"A0", {Zero, {{2500_ns, One}}}},
         {"WE_N", {One, {{1000_ns, Zero}, {2000_ns, One}, {3000_ns, Zero}, {4000_ns, One}}}}},
        BusWord({{{900_ns, 2100_ns}, 0b1111}, {{3150_ns, 4100_ns}, 0b1111}}));
    for (std::size_t word = 0; word < bench.ram->wordCount(); ++word) {
        bench.ram->stick(word, 0, false);
    }
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    CHECK(std::ranges::all_of(std::views::iota(std::size_t{0}, std::size_t{1024}),
                              [&](std::size_t a) { return (bench.ram->word(a) & 1) == 0; }));
    sim.runUntil(4000_ns + delays::k2114Select);
    CHECK(bench.ram->word(0) == 0b1110);
    CHECK(bench.ram->word(1) == 0b1110);
    CHECK(bench.word(sim) == 0b1110);
}
