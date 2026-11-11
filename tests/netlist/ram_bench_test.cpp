// The P4n bench (tests/data/ram_bench.bwn): keyboard → 74LS244 → bus ↔ two 2114s.

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "parts/ram2114.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

std::unique_ptr<LoadedCircuit> LoadBench() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_ROOT_DIR) / "tests/data/ram_bench.bwn");
    REQUIRE(result.has_value());
    return std::move(*result);
}

struct Bench {
    std::unique_ptr<LoadedCircuit> loaded = LoadBench();
    Simulator sim{loaded->circuit};

    explicit Bench(std::uint64_t seed) {
        sim.powerOn(seed);
        sim.runUntil(10_us);
    }

    // SW1 held for 10 µs, then 10 µs to settle.
    void reset() {
        Button* button = ButtonOf(*loaded, "SW1");
        button->setPressed(true);
        sim.wake(button->id());
        sim.runUntil(sim.now() + 10_us);
        button->setPressed(false);
        sim.wake(button->id());
        sim.runUntil(sim.now() + 10_us);
    }

    void type(std::string_view text) {
        Keyboard* keyboard = KeyboardOf(*loaded, "J1");
        for (const char c : text) {
            REQUIRE(keyboard->type(static_cast<std::uint8_t>(c)));
            sim.wake(keyboard->id());
            sim.runUntil(sim.now() + 1_ms);
        }
    }

    int word(std::size_t address) const {
        const auto& low = static_cast<const Ram2114&>(*loaded->components.at("U2"));
        const auto& high = static_cast<const Ram2114&>(*loaded->components.at("U3"));
        return low.word(address) | (high.word(address) & 0b111) << 4;
    }

    // The bus B0-B6 as a number; -1 if a bit is not 0 or 1.
    int bus() const {
        int value = 0;
        for (int bit = 0; bit < 7; ++bit) {
            const LogicValue v = sim.value(loaded->nets.at("B" + std::to_string(bit)));
            if (v != Zero && v != One) {
                return -1;
            }
            value |= (v == One ? 1 : 0) << bit;
        }
        return value;
    }
};

}  // namespace

TEST_CASE("ram bench: loads without warnings", "[netlist]") {
    CHECK(LoadBench()->warnings.empty());
}

TEST_CASE("ram bench: typed keys land in the RAMs and read back", "[netlist]") {
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        INFO(seed);
        Bench b(seed);
        b.reset();
        b.type("HELLO");
        for (std::size_t i = 0; i < 5; ++i) {
            INFO(i);
            CHECK(b.word(i) == "HELLO"[i]);
        }
        CHECK(b.bus() == b.word(5));  // U4 stepped past the last key
        b.reset();
        CHECK(b.bus() == 'H');
    }
}

TEST_CASE("ram bench: the bus is X only briefly as a write starts", "[netlist]") {
    Bench b(4);
    b.reset();
    for (int bit = 0; bit < 7; ++bit) {
        b.sim.record(b.loaded->nets.at("B" + std::to_string(bit)));
    }
    b.sim.record(b.loaded->nets.at("WE_N"));
    b.type("AB");
    std::vector<Time> weFalls;
    for (const Sample& s : b.sim.recorder().samples(b.loaded->nets.at("WE_N"))) {
        if (s.value == Zero) {
            weFalls.push_back(s.time);
        }
    }
    REQUIRE(weFalls.size() == 2);
    for (int bit = 0; bit < 7; ++bit) {
        INFO(bit);
        const auto samples = b.sim.recorder().samples(b.loaded->nets.at("B" + std::to_string(bit)));
        for (std::size_t i = 0; i < samples.size(); ++i) {
            if (samples[i].value != X) {
                continue;
            }
            const bool nearFall = std::ranges::any_of(weFalls, [&](Time fall) {
                return samples[i].time >= fall && samples[i].time < fall + 100_ns;
            });
            CHECK(nearFall);
            REQUIRE(i + 1 < samples.size());
            CHECK(samples[i + 1].time - samples[i].time < 100_ns);
        }
    }
}
