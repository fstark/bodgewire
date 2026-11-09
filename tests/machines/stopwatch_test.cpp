#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "parts/ls47.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "stopwatch.bwn");
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Circuit built from the Board's Copper, with no Faults.
std::unique_ptr<LoadedCircuit> BoardCircuit() {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "stopwatch.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// One digit's shape as text: '0'..'9' as the 74LS47 draws them, ' ' dark, '?' anything else.
char DigitAt(const LoadedCircuit& loaded, const Simulator& sim, std::string_view digit) {
    unsigned lit = 0;
    for (int k = 0; k < 7; ++k) {
        const NetId net = *PinNet(loaded, digit, std::string(1, static_cast<char>('A' + k)));
        if (sim.value(net) == Zero && !sim.isFloating(net)) {
            lit |= 1u << k;
        }
    }
    if (lit == 0) {
        return ' ';
    }
    for (unsigned v = 0; v < 10; ++v) {
        if (Ls47Segments(v, true, true, true) == lit) {
            return static_cast<char>('0' + v);
        }
    }
    return '?';
}

// A powered stopwatch build and its buttons, pressed at exact simulated times.
class Watch {
public:
    Watch(LoadedCircuit& loaded, std::uint64_t seed) : loaded_(loaded), sim_(loaded.circuit) {
        for (const char* button : {"SW1", "SW2", "SW3"}) {
            Button* b = ButtonOf(loaded_, button);
            REQUIRE(b != nullptr);
            b->setPressed(false);
        }
        sim_.powerOn(seed);
    }

    Simulator& sim() { return sim_; }
    Time now() const { return sim_.now(); }
    void runFor(Time time) { sim_.runUntil(sim_.now() + time); }

    // Holds `button` for 500 µs from now.
    void press(std::string_view button) {
        Button* b = ButtonOf(loaded_, button);
        b->setPressed(true);
        sim_.wake(b->id());
        runFor(500_us);
        b->setPressed(false);
        sim_.wake(b->id());
        runFor(1_us);
    }
    void start() { press("SW1"); }
    void stop() { press("SW2"); }
    void reset() { press("SW3"); }

    // "d.dd" from DS1, DS2, DS3.
    std::string display() const {
        return {DigitAt(loaded_, sim_, "DS1"), '.', DigitAt(loaded_, sim_, "DS2"),
                DigitAt(loaded_, sim_, "DS3")};
    }

private:
    LoadedCircuit& loaded_;
    Simulator sim_;
};

// Stop and Reset from power-on (the run flip-flop and counters start random), at 2 ms.
void Clear(Watch& watch) {
    watch.runFor(500_us);
    watch.stop();
    watch.reset();
    watch.sim().runUntil(2_ms);
}

}  // namespace

TEST_CASE("stopwatch: netlist loads without warnings", "[machines]") {
    CHECK(DesignCircuit()->warnings.empty());
}

TEST_CASE("stopwatch: fault-free start, stop, start again", "[machines]") {
    auto loaded = BoardCircuit();
    for (const std::uint64_t seed : {1, 2, 3}) {
        INFO("seed " << seed);
        Watch watch(*loaded, seed);
        Clear(watch);
        CHECK(watch.display() == "0.00");
        watch.start();  // 2 ms
        watch.sim().runUntil(125'400_us);
        watch.stop();  // 123.4 ms counted
        watch.runFor(10_ms);
        CHECK(watch.display() == "0.12");
        watch.start();
        watch.runFor(50_ms - 500_us - 1_us);
        watch.stop();  // 173.4 ms counted
        watch.runFor(1_ms);
        CHECK(watch.display() == "0.17");
    }
}

TEST_CASE("stopwatch: the seconds count", "[machines][slow]") {
    auto loaded = BoardCircuit();
    Watch watch(*loaded, 1);
    Clear(watch);
    watch.start();  // 2 ms
    watch.sim().runUntil(1'057_ms);
    CHECK(watch.display() == "1.05");
}

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

// Clear, Start at 2 ms, Stop at 125.4 ms: the fault-free watch then reads "0.12".
std::string StartStop(LoadedCircuit& loaded, std::uint64_t seed) {
    Watch watch(loaded, seed);
    Clear(watch);
    watch.start();
    watch.sim().runUntil(125'400_us);
    watch.stop();
    watch.runFor(10_ms);
    return watch.display();
}

// The reading just after Stop and Reset.
std::string AfterReset(LoadedCircuit& loaded, std::uint64_t seed) {
    Watch watch(loaded, seed);
    Clear(watch);
    return watch.display();
}

}  // namespace

TEST_CASE("stopwatch: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : {"stuck_at_zero", "nineteen", "eights", "broken_bar", "non_stop",
                             "dark_seconds", "missing_eight"}) {
        INFO(name);
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "stopwatch");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("stopwatch: stuck_at_zero", "[machines]") {
    Machine machine = LoadPuzzle("stuck_at_zero");
    {
        auto loaded = Build(machine);
        Watch watch(*loaded, 1);
        Clear(watch);
        watch.start();  // 2 ms
        watch.sim().runUntil(107_ms);
        CHECK(watch.display() == "0.00");
        CHECK(watch.sim().value(*PinNet(*loaded, "U12", "2Y")) == One);
    }
    REQUIRE(machine.swap("U12"));
    auto loaded = Build(machine);
    Watch watch(*loaded, 1);
    Clear(watch);
    watch.start();
    watch.sim().runUntil(107_ms);
    CHECK(watch.display() == "0.10");
}

TEST_CASE("stopwatch: nineteen", "[machines]") {
    Machine machine = LoadPuzzle("nineteen");
    {
        auto loaded = Build(machine);
        const NetId ckb = *PinNet(*loaded, "U6", "CKB");
        REQUIRE(ckb != *PinNet(*loaded, "U6", "QA"));
        REQUIRE(ckb == *PinNet(*loaded, "U9", "A"));
        for (const std::uint64_t seed : {1, 2, 3}) {
            INFO("seed " << seed);
            Watch watch(*loaded, seed);
            Clear(watch);
            CHECK(watch.display() == "0.10");
            watch.start();  // 2 ms
            watch.sim().runUntil(2_ms + 195_ms);
            CHECK(watch.display() == "0.19");
            CHECK(watch.sim().isFloating(ckb));
            watch.sim().runUntil(2_ms + 205_ms);
            CHECK(watch.display() == "0.10");
        }
    }
    REQUIRE(machine.mend(NineteenCrack().layer, NineteenCrack().at));
    auto loaded = Build(machine);
    Watch watch(*loaded, 1);
    Clear(watch);
    watch.start();
    watch.sim().runUntil(2_ms + 255_ms);
    CHECK(watch.display() == "0.25");
}

TEST_CASE("stopwatch: eights", "[machines]") {
    Machine machine = LoadPuzzle("eights");
    {
        auto loaded = Build(machine);
        CHECK(AfterReset(*loaded, 1) == "0.80");
    }
    REQUIRE(machine.swap("U9"));
    auto loaded = Build(machine);
    CHECK(AfterReset(*loaded, 1) == "0.00");
}

TEST_CASE("stopwatch: broken_bar", "[machines]") {
    Machine machine = LoadPuzzle("broken_bar");
    {
        auto loaded = Build(machine);
        const NetId e = *PinNet(*loaded, "DS1", "E");
        REQUIRE(e != *PinNet(*loaded, "U8", "SE_N"));
        for (const std::uint64_t seed : {1, 2, 3}) {
            INFO("seed " << seed);
            Watch watch(*loaded, seed);
            Clear(watch);
            CHECK(watch.display() == "?.00");  // a 0 without its bottom-left bar
            CHECK(watch.sim().isFloating(e));
        }
    }
    REQUIRE(machine.mend(BrokenBarCrack().layer, BrokenBarCrack().at));
    auto loaded = Build(machine);
    CHECK(AfterReset(*loaded, 1) == "0.00");
}

namespace {

// Clear, Start at 2 ms, Stop at 50 ms, read at 107 ms: a working Stop freezes 0.04.
std::string StopAtFifty(LoadedCircuit& loaded, std::uint64_t seed) {
    Watch watch(loaded, seed);
    Clear(watch);
    watch.start();
    watch.sim().runUntil(50_ms);
    watch.stop();
    watch.sim().runUntil(107_ms);
    return watch.display();
}

}  // namespace

TEST_CASE("stopwatch: non_stop", "[machines]") {
    Machine machine = LoadPuzzle("non_stop");
    {
        auto loaded = Build(machine);
        const NetId gate = *PinNet(*loaded, "U12", "1B");
        REQUIRE(gate != *PinNet(*loaded, "U11", "1Q"));
        for (const std::uint64_t seed : {1, 2, 3}) {
            INFO("seed " << seed);
            // Counting from the end of Reset (1.5 ms), whatever Start and Stop do.
            CHECK(StopAtFifty(*loaded, seed) == "0.10");
        }
    }
    REQUIRE(machine.mend(NonStopCrack().layer, NonStopCrack().at));
    auto loaded = Build(machine);
    CHECK(StopAtFifty(*loaded, 1) == "0.04");
}

TEST_CASE("stopwatch: dark_seconds", "[machines]") {
    Machine machine = LoadPuzzle("dark_seconds");
    CHECK(machine.power().unpowered == std::vector<std::string>{"U5"});
    {
        auto loaded = Build(machine);
        for (const std::uint64_t seed : {1, 2, 3}) {
            INFO("seed " << seed);
            CHECK(StartStop(*loaded, seed) == " .12");
        }
    }
    REQUIRE(machine.mend(DarkSecondsCrack().layer, DarkSecondsCrack().at));
    CHECK(machine.power().unpowered.empty());
    auto loaded = Build(machine);
    CHECK(StartStop(*loaded, 1) == "0.12");
}

namespace {

// The power-on seconds count (S0–S3) and DS1 after 10 µs, for `seed`.
std::pair<unsigned, char> PowerOnSeconds(LoadedCircuit& loaded, std::uint64_t seed) {
    Watch watch(loaded, seed);
    watch.runFor(10_us);
    unsigned count = 0;
    for (int bit = 0; bit < 4; ++bit) {
        const NetId net = *PinNet(loaded, "U5", std::string("Q") + static_cast<char>('A' + bit));
        if (watch.sim().value(net) == One) {
            count |= 1u << bit;
        }
    }
    return {count, DigitAt(loaded, watch.sim(), "DS1")};
}

}  // namespace

TEST_CASE("stopwatch: missing_eight", "[machines]") {
    Machine machine = LoadPuzzle("missing_eight");
    std::uint64_t seed = 0;
    {
        auto loaded = Build(machine);
        CHECK(PinNet(*loaded, "U8", "D") == loaded->nets.at("S3"));  // the leg reads S3
        // Power-on counts are random: find one at 8 or 9 instead of counting for 8 s.
        for (std::uint64_t s = 1; s <= 64 && seed == 0; ++s) {
            const auto [count, digit] = PowerOnSeconds(*loaded, s);
            if (count == 8 || count == 9) {
                seed = s;
                CHECK(digit == static_cast<char>('0' + count - 8));
            }
        }
        REQUIRE(seed != 0);
    }
    REQUIRE(machine.resolder({"U8", "6"}));
    {
        auto loaded = Build(machine);
        const auto [count, digit] = PowerOnSeconds(*loaded, seed);
        CHECK(digit == static_cast<char>('0' + count - 8));
    }
    REQUIRE(machine.swap("U8"));
    auto loaded = Build(machine);
    const auto [count, digit] = PowerOnSeconds(*loaded, seed);
    CHECK(digit == static_cast<char>('0' + count));
}
