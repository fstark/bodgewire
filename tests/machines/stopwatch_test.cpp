#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

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
