// The P5i bench (tests/data/switch_bench.bwn): a 74145 scanning a 4 × 4 key matrix.

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "parts/switch.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

std::string BenchText() {
    std::ifstream in(std::filesystem::path(BW_ROOT_DIR) / "tests/data/switch_bench.bwn");
    REQUIRE(in);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

// `text` with the first `from` replaced by `to`.
std::string Edited(std::string text, std::string_view from, std::string_view to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

// Columns seen low on each row: bit c of row r is set when KEYc is high while row r is scanned.
using Matrix = std::array<unsigned, 4>;

struct Bench {
    std::unique_ptr<LoadedCircuit> loaded;
    std::unique_ptr<Simulator> sim;

    explicit Bench(const std::string& text = BenchText(), std::span<const StuckSwitch> faults = {},
                   std::uint64_t seed = 1) {
        const auto netlist = ParseNetlist(text, "switch_bench.bwn");
        REQUIRE(netlist.has_value());
        auto result = BuildCircuit(*netlist, {.switches = faults});
        REQUIRE(result.has_value());
        loaded = std::move(*result);
        CHECK(loaded->warnings.empty());
        sim = std::make_unique<Simulator>(loaded->circuit);
        sim->powerOn(seed);
        sim->runUntil(10_us);
    }

    LogicValue value(std::string_view net) const {
        return sim->value(loaded->nets.at(std::string(net)));
    }

    void press(std::initializer_list<int> keys, bool down = true) {
        for (const int k : keys) {
            Switch* key = SwitchOf(*loaded, "SW" + std::to_string(k + 1));
            REQUIRE(key != nullptr);
            key->setPressed(down);
            sim->wake(key->id());
        }
        sim->runUntil(sim->now() + 1_us);
    }

    // Eight scan steps, each read in the middle of its 1 µs.
    Matrix scan() {
        Matrix seen{};
        const Time start = (sim->now() / 1_us + 1) * 1_us;
        for (int step = 0; step < 8; ++step) {
            sim->runUntil(start + step * 1_us + 500_ns);
            const unsigned row = (value("QA") == One ? 1u : 0u) | (value("QB") == One ? 2u : 0u);
            for (unsigned c = 0; c < 4; ++c) {
                if (value("KEY" + std::to_string(c)) == One) {
                    seen[row] |= 1u << c;
                }
            }
        }
        return seen;
    }
};

}  // namespace

TEST_CASE("switch bench: no key, every column high", "[netlist]") {
    Bench bench;
    CHECK(bench.scan() == Matrix{});
    for (const char* row : {"ROW0", "ROW1", "ROW2", "ROW3"}) {
        CHECK(bench.loaded->nets.contains(row));
    }
}

TEST_CASE("switch bench: one key shows on its row and column only", "[netlist]") {
    for (int k = 0; k < 16; ++k) {
        Bench bench;
        bench.press({k});
        Matrix expected{};
        expected[static_cast<std::size_t>(k / 4)] = 1u << (k % 4);
        CHECK(bench.scan() == expected);
        bench.press({k}, false);
        CHECK(bench.scan() == Matrix{});
    }
}

TEST_CASE("switch bench: three corners make a ghost at the fourth", "[netlist]") {
    Bench bench;
    bench.press({0, 1, 4});                           // row 0 columns 0 and 1, row 1 column 0
    CHECK(bench.scan() == Matrix{0b11, 0b11, 0, 0});  // key 5 (row 1, column 1) a ghost
    bench.press({1}, false);
    CHECK(bench.scan() == Matrix{0b01, 0b01, 0, 0});
}

TEST_CASE("switch bench: a cracked row leaves its key dead", "[netlist]") {
    // Key 5's row leg on a Net of its own, as a crack between it and ROW1 leaves it.
    Bench bench(Edited(BenchText(), " SW6.1", "") + "net ROW1_CUT SW6.1\n");
    bench.press({5, 4});
    CHECK(bench.scan() == Matrix{0, 0b01, 0, 0});
}

TEST_CASE("switch bench: bridged columns pull together", "[netlist]") {
    // COL1 and COL2 one Net, as a Solder Bridge makes them.
    Bench bench(Edited(BenchText(), "net COL2 ", "net COL1 "));
    bench.press({1});
    CHECK(bench.scan() == Matrix{0b0110, 0, 0, 0});  // key 1 read on columns 1 and 2
}

TEST_CASE("switch bench: switch Faults", "[netlist]") {
    SECTION("stuck closed: seen on every scan without a press") {
        const StuckSwitch faults[] = {{"SW11", true}};  // key 10: row 2, column 2
        Bench bench(BenchText(), faults);
        CHECK(bench.scan() == Matrix{0, 0, 0b100, 0});
    }
    SECTION("never closes: a press does nothing") {
        const StuckSwitch faults[] = {{"SW11", false}};
        Bench bench(BenchText(), faults);
        bench.press({10});
        CHECK(bench.scan() == Matrix{});
    }
}

TEST_CASE("switch bench: the button pulls its line to GND", "[netlist]") {
    for (const std::uint64_t seed : {1u, 2u, 3u}) {
        Bench bench(BenchText(), {}, seed);
        CHECK(bench.value("BTN") == Zero);
        bench.press({16});
        CHECK(bench.value("BTN_N") == Zero);
        CHECK(bench.value("BTN") == One);
        bench.press({16}, false);
        CHECK(bench.value("BTN") == Zero);
    }
}
