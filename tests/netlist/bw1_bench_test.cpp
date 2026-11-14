// The P5j bench (tests/data/bw1_bench.bwn): the KIM-1's 6502, 74145, 6530-002 and 1K RAM of
// 6102s through 74125s, running roms/bw1_bench.asm.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "netlist/parser.hpp"
#include "parts/mos6530.hpp"
#include "parts/ram6102.hpp"
#include "parts/switch.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kDone = 28_ms;     // the RAM tests are over by then (about 23 ms)
constexpr Time kPeriod = 800_us;  // TICKS x 8 cycles at 1 MHz
// The handler's latency, about 16-24 us (design §9): accepted from 10 to 30 us.
constexpr Time kLatencyMin = 10_us, kLatencyMax = 30_us;

std::filesystem::path BenchPath() {
    return std::filesystem::path(BW_ROOT_DIR) / "tests/data/bw1_bench.bwn";
}

struct Bench {
    std::unique_ptr<LoadedCircuit> loaded;
    std::unique_ptr<Simulator> sim;

    explicit Bench(std::uint64_t seed, std::span<const StuckCell> cells = {}) {
        std::ifstream in(BenchPath());
        REQUIRE(in);
        std::stringstream text;
        text << in.rdbuf();
        auto netlist = ParseNetlist(text.str(), "bw1_bench.bwn");
        REQUIRE(netlist.has_value());
        ResolveContentsPaths(*netlist, BenchPath().parent_path());
        auto result = BuildCircuit(*netlist, {}, {}, cells);
        REQUIRE(result.has_value());
        loaded = std::move(*result);
        CHECK(loaded->warnings.empty());
        sim = std::make_unique<Simulator>(loaded->circuit);
        sim->powerOn(seed);
        sim->record(net("IRQ_N"));  // after powerOn, which clears the recorder
    }

    NetId net(const std::string& name) const { return loaded->nets.at(name); }
    LogicValue value(const std::string& name) const { return sim->value(net(name)); }

    // Bits `first`..`first + count - 1` of a port, from its Nets; -1 if one is not 0 or 1.
    int port(char which, unsigned first, unsigned count) const {
        int result = 0;
        for (unsigned bit = 0; bit < count; ++bit) {
            const LogicValue v = value(std::string("P") + which + std::to_string(first + bit));
            if (v != Zero && v != One) {
                return -1;
            }
            result |= (v == One ? 1 : 0) << bit;
        }
        return result;
    }

    std::vector<Time> pb7Falls(Time from) const {
        std::vector<Time> falls;
        LogicValue last = One;
        for (const Sample& s : sim->recorder().samples(net("IRQ_N"))) {
            if (s.time >= from && last == One && s.value == Zero) {
                falls.push_back(s.time);
            }
            last = s.value;
        }
        return falls;
    }
};

// A Pin's index on a Part of the bench.
PinIndex PinOf(const Bench& bench, const std::string& reference, std::string_view pin) {
    return bench.loaded->components.at(reference)->pinIndex(pin);
}

}  // namespace

TEST_CASE("bw1 bench: RAM tests pass, the timer interrupts, the button reads", "[netlist][slow]") {
    for (const std::uint64_t seed : {1u, 2u, 3u}) {
        INFO(seed);
        Bench bench(seed);
        bench.sim->runUntil(kDone);
        CHECK(bench.port('A', 0, 7) == 0x55);
        const int count = bench.port('B', 0, 5);
        bench.sim->runUntil(kDone + 5_ms);
        const auto falls = bench.pb7Falls(kDone);
        REQUIRE(falls.size() >= 5);
        for (std::size_t i = 1; i < falls.size(); ++i) {
            const Time gap = falls[i] - falls[i - 1];
            CHECK(gap >= kPeriod + kLatencyMin);
            CHECK(gap <= kPeriod + kLatencyMax);
        }
        const int later = bench.port('B', 0, 5);
        CHECK(later == ((count + static_cast<int>(falls.size())) & 0x1F));
        CHECK(bench.value("PB5") == One);  // the button up: PA7 pulled up by the 6530

        Switch* button = SwitchOf(*bench.loaded, "SW1");
        REQUIRE(button != nullptr);
        button->setPressed(true);
        bench.sim->wake(button->id());
        bench.sim->runUntil(bench.sim->now() + 50_us);
        CHECK(bench.value("PA7") == Zero);
        CHECK(bench.value("PB5") == Zero);
        button->setPressed(false);
        bench.sim->wake(button->id());
        bench.sim->runUntil(bench.sim->now() + 50_us);
        CHECK(bench.value("PB5") == One);
    }
}

TEST_CASE("bw1 bench: a stuck 6102 cell fails the first test", "[netlist][slow]") {
    // Word $123 holds $23 ^ $FF = $DC: D7 (U5) is 1 there; stuck at 0.
    for (const std::uint64_t seed : {1u, 2u, 3u}) {
        INFO(seed);
        Bench probe(seed);
        const StuckCell cells[] = {{"U5", PinOf(probe, "U5", "DIN"), std::size_t{0x123}, Zero}};
        Bench bench(seed, cells);
        bench.sim->runUntil(kDone);
        CHECK(bench.port('A', 0, 7) == 0x01);
    }
}

TEST_CASE("bw1 bench: a stuck cell in the 6530's RAM fails the second test", "[netlist][slow]") {
    // RAM word $10 holds $10 ^ $A5 = $B5: D3 is 0 there; stuck at 1.
    for (const std::uint64_t seed : {1u, 2u, 3u}) {
        INFO(seed);
        Bench probe(seed);
        const StuckCell cells[] = {{"U2", PinOf(probe, "U2", "D3"), std::size_t{0x10}, One, 1}};
        Bench bench(seed, cells);
        bench.sim->runUntil(kDone);
        CHECK(bench.port('A', 0, 7) == 0x02);
    }
}
