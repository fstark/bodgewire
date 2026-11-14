#include "parts/ram6102.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
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

// One 6102, every input driven by a Stimulus (listed, or 0); DOUT on a Net of its own.
struct Bench {
    Circuit circuit;
    Ram6102* ram = nullptr;
    NetId out{};

    explicit Bench(const std::map<std::string, Input>& inputs) {
        ram = &circuit.add<Ram6102>("U1");
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
        out = circuit.addNet("DOUT");
        circuit.connect(*ram, Ram6102::kDout, out);
        circuit.finalize();
    }

    // DOUT: 0, 1, or -1 when floating or X.
    int bit(const Simulator& sim) const {
        const LogicValue v = sim.value(out);
        if (sim.isFloating(out) || (v != Zero && v != One)) {
            return -1;
        }
        return v == One ? 1 : 0;
    }
};

}  // namespace

TEST_CASE("6102: catalogue row and pins", "[parts]") {
    const PartInfo* part = FindPart("6102");
    REQUIRE(part != nullptr);
    CHECK(part->package == "DIP-16");
    CHECK(part->kind == PartKind::Chip);

    Ram6102 ram("U1");
    const auto pins = ram.pins();
    REQUIRE(pins.size() == 16);
    const std::array<std::string_view, 16> names{"A6",   "A5", "RW",  "A1",  "A2",  "A3",
                                                 "A4",   "A0", "GND", "VCC", "DIN", "DOUT",
                                                 "CE_N", "A9", "A8",  "A7"};
    for (std::size_t i = 0; i < names.size(); ++i) {
        INFO(i);
        CHECK(pins[i].name == names[i]);
        if (names[i] == "GND" || names[i] == "VCC") {
            CHECK(pins[i].kind == PinKind::Power);
            continue;
        }
        CHECK(pins[i].family == Family::Mos);
        CHECK(pins[i].kind == (names[i] == "DOUT" ? PinKind::Output : PinKind::Input));
        CHECK(pins[i].sampled == (names[i] == "DIN"));
    }
    for (std::size_t bit = 0; bit < 10; ++bit) {
        CHECK(pins[Ram6102::kAddress[bit]].name == "A" + std::to_string(bit));
    }
    CHECK(pins[Ram6102::kRw].name == "RW");
    CHECK(pins[Ram6102::kDin].name == "DIN");
    CHECK(pins[Ram6102::kDout].name == "DOUT");
    CHECK(pins[Ram6102::kCeN].name == "CE_N");
}

TEST_CASE("6102: writes, reads and their delays", "[parts]") {
    // Address 5, then 6 from 2.5 us, 5 again from 4.5 us. Writes 1 at 5 (1-2 us) and 0 at 6
    // (3-4 us). CE_N high from 6 us to 7 us.
    Bench bench({{"A0", {One, {{2500_ns, Zero}, {4500_ns, One}}}},
                 {"A1", {Zero, {{2500_ns, One}, {4500_ns, Zero}}}},
                 {"A2", {One, {}}},
                 {"RW", {One, {{1000_ns, Zero}, {2000_ns, One}, {3000_ns, Zero}, {4000_ns, One}}}},
                 {"DIN", {One, {{2500_ns, Zero}}}},
                 {"CE_N", {Zero, {{6000_ns, One}, {7000_ns, Zero}}}}});
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    // Writing: DOUT off k6102Off after RW falls; back on k6102Select after it rises.
    sim.runUntil(1000_ns + delays::k6102Off);
    CHECK(bench.bit(sim) == -1);
    sim.runUntil(2010_ns);
    CHECK(bench.ram->bit(5));
    sim.runUntil(2000_ns + delays::k6102Select - 1);
    CHECK(bench.bit(sim) == -1);
    sim.runUntil(2000_ns + delays::k6102Select);
    CHECK(bench.bit(sim) == 1);
    sim.runUntil(4000_ns + delays::k6102Select);
    CHECK_FALSE(bench.ram->bit(6));
    CHECK(bench.bit(sim) == 0);
    // Address 6 → 5 at 4.5 us: k6102Access later.
    sim.runUntil(4500_ns + delays::k6102Access - 1);
    CHECK(bench.bit(sim) == 0);
    sim.runUntil(4500_ns + delays::k6102Access);
    CHECK(bench.bit(sim) == 1);
    // Deselected: off k6102Off later; selected again: driven k6102Select later.
    sim.runUntil(6000_ns + delays::k6102Off - 1);
    CHECK(bench.bit(sim) == 1);
    sim.runUntil(6000_ns + delays::k6102Off);
    CHECK(bench.bit(sim) == -1);
    sim.runUntil(7000_ns + delays::k6102Select - 1);
    CHECK(bench.bit(sim) == -1);
    sim.runUntil(7000_ns + delays::k6102Select);
    CHECK(bench.bit(sim) == 1);
}

TEST_CASE("6102: a write keeps the address and data held at its end", "[parts]") {
    // DIN 1 then 0 during the write at 1; the address goes 1 → 2 as RW rises: 1 is written.
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        INFO(seed);
        Bench bench({{"A0", {One, {{2000_ns, Zero}}}},
                     {"A1", {Zero, {{2000_ns, One}}}},
                     {"DIN", {One, {{1500_ns, Zero}}}},
                     {"RW", {One, {{1000_ns, Zero}, {2000_ns, One}}}}});
        Simulator sim(bench.circuit);
        sim.powerOn(seed);
        const bool before = bench.ram->bit(2);
        sim.runUntil(3000_ns);
        CHECK_FALSE(bench.ram->bit(1));
        CHECK(bench.ram->bit(2) == before);
    }
}

TEST_CASE("6102: random contents at power-on, set by the seed", "[parts]") {
    const auto contents = [](std::uint64_t seed) {
        Bench bench({{"CE_N", {One, {}}}});
        Simulator sim(bench.circuit);
        sim.powerOn(seed);
        std::vector<bool> bits;
        for (std::size_t a = 0; a < 1024; ++a) {
            bits.push_back(bench.ram->bit(a));
        }
        return bits;
    };
    const auto one = contents(1);
    CHECK(contents(1) == one);
    CHECK(contents(2) != one);
    CHECK(std::ranges::count(one, true) > 0);
    CHECK(std::ranges::count(one, false) > 0);
}

TEST_CASE("6102: a stuck cell holds its value through writes and power-on", "[parts]") {
    // Writes 1 at 3 (1-2 us), then reads it from 2 us.
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        INFO(seed);
        Bench bench({{"A0", {One, {}}},
                     {"A1", {One, {}}},
                     {"DIN", {One, {}}},
                     {"RW", {One, {{1000_ns, Zero}, {2000_ns, One}}}}});
        CHECK(bench.ram->wordCount(0) == 1024);
        CHECK(bench.ram->dataBit(Ram6102::kDin) == 0u);
        CHECK(bench.ram->dataBit(Ram6102::kDout) == 0u);
        CHECK_FALSE(bench.ram->dataBit(Ram6102::kAddress[0]).has_value());
        bench.ram->stick(0, 3, 0, false);
        Simulator sim(bench.circuit);
        sim.powerOn(seed);
        CHECK_FALSE(bench.ram->bit(3));
        sim.runUntil(2000_ns + delays::k6102Select);
        CHECK_FALSE(bench.ram->bit(3));
        CHECK(bench.bit(sim) == 0);
    }
}
