#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../machine/test_points.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "parts/switch.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kCycle = 1_us;  // 1 MHz

// Built from the routed Board (the Machine as the player gets it).
std::unique_ptr<LoadedCircuit> BoardCircuit() {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "heartbeat.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

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

// U1's pins, LA14 and LA15 recorded from a seeded power-on.
struct Run {
    std::unique_ptr<LoadedCircuit> loaded;
    std::map<std::string, NetId> nets;
    std::optional<Simulator> sim;

    explicit Run(std::uint64_t seed, std::unique_ptr<LoadedCircuit> circuit = BoardCircuit())
        : loaded(std::move(circuit)) {
        for (int bit = 0; bit < 16; ++bit) {
            const std::string name = "A" + std::to_string(bit);
            nets[name] = CpuNet(*loaded, name);
        }
        for (const char* name : {"PHI0", "SYNC", "RW", "RES_N"}) {
            nets[name] = CpuNet(*loaded, name);
        }
        const auto anode = PinNet(*loaded, "D16", "A");  // LA15
        REQUIRE(anode.has_value());
        nets["LA15"] = *anode;
        const auto anode14 = PinNet(*loaded, "D15", "A");  // LA14
        REQUIRE(anode14.has_value());
        nets["LA14"] = *anode14;
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
    // Times `name` went to `level` after `from`.
    std::vector<Time> edges(const std::string& name, LogicValue level, Time from) const {
        std::vector<Time> times;
        for (const Sample& s : sim->recorder().samples(nets.at(name))) {
            if (s.time > from && s.value == level) {
                times.push_back(s.time);
            }
        }
        return times;
    }
    std::vector<Time> falls(Time from) const { return edges("PHI0", Zero, from); }
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
    Switch* button = SwitchOf(*run.loaded, "SW1");
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

// Puzzles (HEARTBEAT.md, Puzzles): each from power-on, faulty, then repaired.

namespace {

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

std::unique_ptr<LoadedCircuit> Build(const Machine& machine) {
    auto built = machine.build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// The address goes up by one every 2 µs, as in "the address counts".
bool Counts(const Run& run, Time from) {
    const auto falls = run.falls(from);
    if (falls.size() < 20) {
        return false;
    }
    for (std::size_t i = 0; i + 2 < falls.size(); ++i) {
        const Time t = falls[i] + 500'000, next = falls[i + 1] + 500'000;
        const bool sync = run.at("SYNC", t) == One;
        if (run.at("RW", t) != One ||
            run.address(next) != static_cast<std::uint16_t>(run.address(t) + (sync ? 1 : 0))) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("heartbeat: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : {"flatline", "fifteen", "flutter"}) {
        INFO(name);
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "heartbeat");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("heartbeat: flatline", "[machines]") {
    Machine machine = LoadPuzzle("flatline");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Run run(seed, Build(machine));
        const auto oscillator = PinNet(*run.loaded, "X1", "OUT");
        REQUIRE(oscillator.has_value());
        CHECK(*oscillator != run.nets.at("PHI0"));  // U1's clock pin cut off X1
        run.sim->runUntil(200_us);
        CHECK(run.falls(1_us).empty());
        CHECK(run.address(10_us) == run.address(200_us));
    }
    REQUIRE(machine.mend(FlatlineCrack().layer, FlatlineCrack().at));
    Run run(1, Build(machine));
    run.sim->runUntil(100_us);
    CHECK(Counts(run, 10_us));
}

TEST_CASE("heartbeat: fifteen", "[machines]") {
    Machine machine = LoadPuzzle("fifteen");
    {
        Run run(1, Build(machine));
        run.sim->runUntil(100_ms);
        CHECK(run.edges("A14", Zero, 1_us).size() >= 1);  // the CPU counts through A14
        CHECK(run.edges("A14", One, 1_us).size() >= 1);
        CHECK(run.at("LA14", 2_us) == One);
        CHECK(run.edges("LA14", Zero, 1_us).empty());  // D15 never goes out
        CHECK(run.edges("LA15", One, 1_us).size() + run.edges("LA15", Zero, 1_us).size() >= 1);
    }
    REQUIRE(machine.mend(FifteenCrack().layer, FifteenCrack().at));
    Run run(1, Build(machine));
    run.sim->runUntil(100_ms);
    CHECK(run.edges("LA14", Zero, 1_us).size() >= 1);
    CHECK(run.edges("LA14", One, 1_us).size() >= 1);
}

TEST_CASE("heartbeat: flutter", "[machines]") {
    Machine machine = LoadPuzzle("flutter");
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Run run(seed, Build(machine));
        run.sim->runUntil(200_us);
        CHECK_FALSE(run.edges("RW", Zero, 10_us).empty());  // INC writes
        // Each INC: opcode, two operand bytes, then $EEEE read and written twice.
        int atEEEE = 0, cycles = 0;
        for (const Time fall : run.falls(10_us)) {
            if (fall + 500'000 > 200_us) {
                break;
            }
            ++cycles;
            atEEEE += run.address(fall + 500'000) == 0xEEEE ? 1 : 0;
        }
        CHECK(cycles > 150);
        CHECK(atEEEE * 2 >= cycles - 6);
    }
    REQUIRE(machine.swap("U1"));
    Run run(1, Build(machine));
    run.sim->runUntil(100_us);
    CHECK(Counts(run, 10_us));
}
