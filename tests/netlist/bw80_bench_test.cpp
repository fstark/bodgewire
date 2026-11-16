// The P5u bench (tests/data/bw80_bench.bwn): the ZX80's Z80, 2332, 2114s behind the data bus
// resistors, NOP gates, 74LS373 and 74LS365, running machines/140-bw80/roms/bw80_bench.asm.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "netlist/parser.hpp"
#include "parts/ram2114.hpp"
#include "parts/switch.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kDone = 56_ms;  // STATUS is written before 53 ms (design §6.3)
constexpr std::uint16_t kStatus = 0x4000, kKeys8 = 0x4001, kKeys9 = 0x4002;

std::filesystem::path BenchPath() {
    return std::filesystem::path(BW_ROOT_DIR) / "tests/data/bw80_bench.bwn";
}

struct Bench {
    std::unique_ptr<LoadedCircuit> loaded;
    std::unique_ptr<Simulator> sim;

    explicit Bench(std::uint64_t seed, std::span<const StuckCell> cells = {},
                   std::vector<std::string> recorded = {}) {
        std::ifstream in(BenchPath());
        REQUIRE(in);
        std::stringstream text;
        text << in.rdbuf();
        auto netlist = ParseNetlist(text.str(), "bw80_bench.bwn");
        REQUIRE(netlist.has_value());
        ResolveContentsPaths(*netlist, BenchPath().parent_path());
        auto result = BuildCircuit(*netlist, {}, {}, cells);
        REQUIRE(result.has_value());
        loaded = std::move(*result);
        CHECK(loaded->warnings.empty());
        sim = std::make_unique<Simulator>(loaded->circuit);
        sim->powerOn(seed);
        for (const std::string& name : recorded) {
            sim->record(net(name));  // after powerOn, which clears the recorder
        }
    }

    NetId net(const std::string& name) const { return loaded->nets.at(name); }
    const Ram2114& ram(const std::string& reference) const {
        return static_cast<const Ram2114&>(*loaded->components.at(reference));
    }

    // A RAM byte, from the two 2114s in the ZX80's bit order: IC3's IO1-IO4 hold D7, D1, D0, D2;
    // IC4's D4, D3, D5, D6.
    unsigned readRam(std::uint16_t address) const {
        const std::size_t word = address & 0x3FF;
        const unsigned low = ram("IC3").word(word), high = ram("IC4").word(word);
        const auto bit = [](unsigned nibble, unsigned i) { return (nibble >> i) & 1u; };
        return bit(low, 2) << 0 | bit(low, 1) << 1 | bit(low, 3) << 2 | bit(high, 1) << 3 |
               bit(high, 0) << 4 | bit(high, 2) << 5 | bit(high, 3) << 6 | bit(low, 0) << 7;
    }

    // A recorded Net's value at `t`: its last sample at or before.
    LogicValue at(const std::string& name, Time t) const {
        LogicValue value = X;
        for (const Sample& s : sim->recorder().samples(net(name))) {
            if (s.time > t) {
                break;
            }
            value = s.value;
        }
        return value;
    }

    // Eight recorded Nets `prefix`0-7 at `t` as a byte; -1 if one is not 0 or 1.
    int byteAt(const std::string& prefix, Time t, unsigned bits = 8) const {
        int value = 0;
        for (unsigned i = 0; i < bits; ++i) {
            const LogicValue v = at(prefix + std::to_string(i), t);
            if (v != Zero && v != One) {
                return -1;
            }
            value |= (v == One ? 1 : 0) << i;
        }
        return value;
    }

    // Falls of a recorded Net.
    std::vector<Time> falls(const std::string& name) const {
        std::vector<Time> times;
        LogicValue last = One;
        for (const Sample& s : sim->recorder().samples(net(name))) {
            if (last == One && s.value == Zero) {
                times.push_back(s.time);
            }
            last = s.value;
        }
        return times;
    }
};

std::vector<std::string> Bus(const std::string& prefix) {
    std::vector<std::string> names;
    for (int i = 0; i < 8; ++i) {
        names.push_back(prefix + std::to_string(i));
    }
    return names;
}

}  // namespace

TEST_CASE("bw80 bench: the program passes", "[netlist][slow]") {
    for (const std::uint64_t seed : {1u, 2u, 3u}) {
        INFO(seed);
        Bench bench(seed);
        bench.sim->runUntil(kDone);
        CHECK(bench.readRam(kStatus) == 0x55);
    }
}

TEST_CASE("bw80 bench: the latch holds each character code", "[netlist][slow]") {
    std::vector<std::string> recorded = Bus("CH");
    recorded.insert(recorded.end(), {"RFSH_N", "A15"});
    Bench bench(1, {}, recorded);
    bench.sim->runUntil(kDone);
    REQUIRE(bench.readRam(kStatus) == 0x55);
    // Each refresh after a fetch in the mirror (A15 high 200 ns before, in T2): the code latched.
    std::vector<int> codes;
    std::vector<LogicValue> bit6;
    for (const Time t : bench.falls("RFSH_N")) {
        if (bench.at("A15", t - 200'000) == One) {
            codes.push_back(bench.byteAt("CH", t, 6));
            bit6.push_back(bench.at("CH6", t));
        }
    }
    REQUIRE(codes.size() == 17);
    for (int i = 0; i < 16; ++i) {
        CHECK(codes[static_cast<std::size_t>(i)] == i + 1);
        CHECK(bit6[static_cast<std::size_t>(i)] == Zero);
    }
    CHECK(codes[16] == (0xC9 & 0x3F));  // the RET, read as itself
    CHECK(bit6[16] == One);
}

TEST_CASE("bw80 bench: a write into the ROM leaves the CPU side clean", "[netlist][slow]") {
    std::vector<std::string> recorded = Bus("D");
    const auto memory = Bus("MD");
    recorded.insert(recorded.end(), memory.begin(), memory.end());
    recorded.insert(recorded.end(), {"WR_N", "ROM_CS_N"});
    Bench bench(1, {}, recorded);
    bench.sim->runUntil(kDone);
    REQUIRE(bench.readRam(kStatus) == 0x55);
    int writes = 0;
    const auto wr = bench.sim->recorder().samples(bench.net("WR_N"));
    for (std::size_t i = 0; i + 1 < wr.size(); ++i) {
        if (wr[i].value != Zero) {
            continue;
        }
        const Time from = wr[i].time, to = wr[i + 1].time, mid = (from + to) / 2;
        if (bench.at("ROM_CS_N", mid) != Zero) {
            continue;  // a RAM write
        }
        ++writes;
        CHECK(bench.byteAt("D", mid) == 0xAA);
        CHECK(bench.byteAt("MD", mid) == 0x3C);
        for (int bit = 0; bit < 8; ++bit) {
            const std::string name = "D" + std::to_string(bit);
            for (const Sample& s : bench.sim->recorder().samples(bench.net(name))) {
                if (s.time >= from && s.time <= to) {
                    CHECK(s.value != X);
                }
            }
        }
    }
    CHECK(writes == 1);
}

TEST_CASE("bw80 bench: keys", "[netlist][slow]") {
    Bench bench(1);
    bench.sim->runUntil(kDone + 2_ms);
    REQUIRE(bench.readRam(kStatus) == 0x55);
    // D5 floats on a keyboard read; D6 high through R7 and R28, D7 low through R1.
    CHECK((bench.readRam(kKeys8) & 0xDF) == 0x5F);
    CHECK((bench.readRam(kKeys9) & 0xDF) == 0x5F);

    Switch* key = SwitchOf(*bench.loaded, "K1");
    REQUIRE(key != nullptr);
    key->setPressed(true);
    bench.sim->wake(key->id());
    bench.sim->runUntil(bench.sim->now() + 2_ms);
    CHECK((bench.readRam(kKeys8) & 0xDF) == 0x5E);
    CHECK((bench.readRam(kKeys9) & 0xDF) == 0x5F);
    key->setPressed(false);
    bench.sim->wake(key->id());
    bench.sim->runUntil(bench.sim->now() + 2_ms);
    CHECK((bench.readRam(kKeys8) & 0xDF) == 0x5F);
}

TEST_CASE("bw80 bench: a stuck RAM cell fails the RAM test", "[netlist][slow]") {
    // Bit 0 of $4005 (IC3's IO3) stuck at 1: the complement pass reads $BB for $BA.
    Bench probe(1);
    const PinIndex io3 = probe.loaded->components.at("IC3")->pinIndex("IO3");
    const StuckCell cells[] = {{"IC3", io3, std::size_t{5}, One}};
    Bench bench(1, cells);
    bench.sim->runUntil(kDone);
    CHECK(bench.readRam(kStatus) == 0xE1);
}
