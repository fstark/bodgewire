#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "netlist/builder.hpp"
#include "parts/button.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kCycle = 1_us;  // 1 MHz

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "heartbeat.bwn");
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Net of U1's pin `name` (connected or not).
NetId CpuNet(const LoadedCircuit& loaded, const std::string& name) {
    const Component& cpu = *loaded.components.at("U1");
    return loaded.circuit.netOf(PinId{ToIndex(cpu.firstPin()) + cpu.pinIndex(name)});
}

// U1's pins and LA15 recorded from a seeded power-on.
struct Run {
    std::unique_ptr<LoadedCircuit> loaded = DesignCircuit();
    std::map<std::string, NetId> nets;
    std::optional<Simulator> sim;

    explicit Run(std::uint64_t seed) {
        for (int bit = 0; bit < 16; ++bit) {
            const std::string name = "A" + std::to_string(bit);
            nets[name] = CpuNet(*loaded, name);
        }
        for (const char* name : {"PHI0", "SYNC", "RW", "RES_N"}) {
            nets[name] = CpuNet(*loaded, name);
        }
        nets["LA15"] = loaded->nets.at("LA15");
        sim.emplace(loaded->circuit);
        sim->powerOn(seed);
        for (const auto& [name, net] : nets) {
            sim->record(net);
        }
    }
    LogicValue at(const std::string& name, Time time) const {
        LogicValue value = X;
        for (const Sample& s : sim->recorder().samples(nets.at(name))) {
            if (s.time > time) {
                break;
            }
            value = s.value;
        }
        return value;
    }
    std::uint16_t address(Time time) const {
        unsigned value = 0;
        for (int bit = 0; bit < 16; ++bit) {
            value |= (at("A" + std::to_string(bit), time) == One ? 1u : 0u) << bit;
        }
        return static_cast<std::uint16_t>(value);
    }
    std::vector<Time> falls(Time from) const {
        std::vector<Time> times;
        for (const Sample& s : sim->recorder().samples(nets.at("PHI0"))) {
            if (s.time > from && s.value == Zero) {
                times.push_back(s.time);
            }
        }
        return times;
    }
};

}  // namespace

TEST_CASE("heartbeat: netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    for (const Diagnostic& d : loaded->warnings) {
        UNSCOPED_INFO(Format(d));
    }
    CHECK(loaded->warnings.empty());
}

TEST_CASE("heartbeat: the address counts", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    Run run(seed);
    run.sim->runUntil(210_us);
    const auto falls = run.falls(10_us);
    REQUIRE(falls.size() > 190);
    // Each cycle read 500 ns after its fall: fetch at P (SYNC), dummy read at P + 1, fetch at
    // P + 1, …; nothing written.
    for (std::size_t i = 0; i + 2 < falls.size(); ++i) {
        const Time t = falls[i] + 500'000, next = falls[i + 1] + 500'000;
        INFO("cycle at " << t);
        CHECK(run.at("RW", t) == One);
        const bool sync = run.at("SYNC", t) == One;
        CHECK((run.at("SYNC", next) == One) != sync);
        const auto expected = static_cast<std::uint16_t>(run.address(t) + (sync ? 1 : 0));
        CHECK(run.address(next) == expected);
    }
}

TEST_CASE("heartbeat: the top LED blinks", "[machines][slow]") {
    Run run(1);
    run.sim->runUntil(300_ms);
    std::vector<Sample> changes;
    for (const Sample& s : run.sim->recorder().samples(run.nets.at("LA15"))) {
        if (s.time > 1_us) {  // not the buffer settling from its random power-on output
            changes.push_back(s);
        }
    }
    REQUIRE(changes.size() >= 3);
    const auto near = [](Time a, Time b) { return a - b < 10_us && b - a < 10_us; };
    for (std::size_t i = 0; i + 1 < changes.size(); ++i) {
        INFO("change at " << changes[i].time);
        CHECK(near(changes[i + 1].time - changes[i].time, 65'536_us));  // high or low, half each
    }
    for (std::size_t i = 0; i + 2 < changes.size(); ++i) {
        CHECK(near(changes[i + 2].time - changes[i].time, 131'072_us));
    }
}

TEST_CASE("heartbeat: reset starts at EAEA", "[machines]") {
    Run run(1);
    Button* button = ButtonOf(*run.loaded, "SW1");
    REQUIRE(button != nullptr);
    run.sim->runUntil(50_us);
    button->setPressed(true);
    run.sim->wake(button->id());
    run.sim->runUntil(55_us);
    button->setPressed(false);
    run.sim->wake(button->id());
    run.sim->runUntil(80_us);
    // The first fall that sees RES_N high ends the reset sequence's first cycle.
    Time seen = 0;
    for (const Time fall : run.falls(55_us)) {
        if (run.at("RES_N", fall - 1) == One) {
            seen = fall;
            break;
        }
    }
    REQUIRE(seen > 0);
    const std::uint16_t pc = run.address(seen - 500'000);
    const auto cycleAt = [&](int k) { return seen + k * kCycle + 500'000; };
    CHECK(run.address(cycleAt(0)) == pc);
    for (int k = 1; k <= 3; ++k) {
        CHECK((run.address(cycleAt(k)) & 0xFF00) == 0x0100);
    }
    CHECK(run.address(cycleAt(4)) == 0xFFFC);
    CHECK(run.address(cycleAt(5)) == 0xFFFD);
    CHECK(run.address(cycleAt(6)) == 0xEAEA);
    CHECK(run.at("SYNC", cycleAt(6)) == One);
    for (int k = 0; k <= 6; ++k) {
        CHECK(run.at("RW", cycleAt(k)) == One);
    }
}
