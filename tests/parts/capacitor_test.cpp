#include "parts/capacitor.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "netlist/diagnostic.hpp"
#include "sim/circuit.hpp"
#include "sim/fixtures.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

constexpr Time kWidth = 22'090;  // 470 ohms x 47 pF

// A Stimulus on A, a Capacitor from A to B, B pulled to `pull` (Weak).
struct Bench {
    Circuit circuit;
    NetId a{}, b{};

    Bench(std::vector<Stimulus::Step> steps, LogicValue pull) {
        a = circuit.addNet("A");
        b = circuit.addNet("B");
        auto& source = circuit.add<Stimulus>("S", Zero, std::move(steps));
        circuit.connect(source, Stimulus::kOut, a);
        auto& capacitor = circuit.add<Capacitor>("C1", 0, kWidth);
        circuit.connect(capacitor, 0, a);
        circuit.connect(capacitor, 1, b);
        circuit.tie(b, pull, DriveStrength::Weak);
        circuit.finalize();
    }
};

std::unique_ptr<LoadedCircuit> Load(std::string_view text) {
    auto result = LoadNetlist(text, "t.bwn");
    if (!result) {
        for (const Diagnostic& d : result.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(result.has_value());
    return std::move(*result);
}

std::vector<std::string> Errors(std::string_view text) {
    const auto result = LoadNetlist(text, "t.bwn");
    REQUIRE_FALSE(result.has_value());
    std::vector<std::string> errors;
    for (const Diagnostic& d : result.error()) {
        errors.push_back(std::to_string(d.line) + ": " + d.message);
    }
    return errors;
}

// A 1 MHz oscillator on A, C1 to B with `rest` (the pulls and Parts on B), B read by an inverter.
std::string CouplingNetlist(std::string_view capacitor, std::string_view rest) {
    return "part X1 OSC freq=1MHz\npart U1 74LS04\npart C1 C " + std::string(capacitor) +
           "\nnet A X1.OUT C1.1\nnet B C1.2 U1.1A\n" + std::string(rest) +
           "net GND U1.2A U1.3A U1.4A U1.5A U1.6A\n";
}

// B's high time in the first 2 us, scanned every ns: the pulses of A's rises.
Time HighTime(LoadedCircuit& loaded) {
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    const NetId b = loaded.nets.at("B");
    Time high = 0;
    for (Time t = 1'000; t <= 2'000'000; t += 1'000) {
        sim.runUntil(t);
        if (!sim.isFloating(b) && sim.value(b) == One) {
            high += 1'000;
        }
    }
    return high;
}

}  // namespace

TEST_CASE("capacitor: a rise is a high pulse on a pulled-down line", "[parts]") {
    Bench bench({{1_us, One}, {2_us, Zero}}, Zero);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us);
    CHECK(sim.value(bench.b) == Zero);
    sim.runUntil(1_us + kCouplingDelay);
    CHECK(sim.value(bench.b) == One);
    sim.runUntil(1_us + kWidth + kCouplingDelay - 1);
    CHECK(sim.value(bench.b) == One);
    sim.runUntil(1_us + kWidth + kCouplingDelay);
    CHECK(sim.value(bench.b) == Zero);
    // The fall: 0 on a line already low.
    sim.runUntil(2_us + 10_ns);
    CHECK(sim.value(bench.b) == Zero);
}

TEST_CASE("capacitor: a fall is a low pulse on a pulled-up line", "[parts]") {
    Bench bench({{1_us, One}, {2_us, Zero}}, One);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us + 10_ns);
    CHECK(sim.value(bench.b) == One);
    sim.runUntil(2_us + kCouplingDelay);
    CHECK(sim.value(bench.b) == Zero);
    sim.runUntil(2_us + kWidth + kCouplingDelay - 1);
    CHECK(sim.value(bench.b) == Zero);
    sim.runUntil(2_us + kWidth + kCouplingDelay);
    CHECK(sim.value(bench.b) == One);
}

TEST_CASE("capacitor: X passes as X, then the pull", "[parts]") {
    Bench bench({{1_us, X}}, Zero);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(1_us + kCouplingDelay);
    CHECK(sim.value(bench.b) == X);
    sim.runUntil(1_us + kWidth + kCouplingDelay);
    CHECK(sim.value(bench.b) == Zero);
}

TEST_CASE("capacitor: an edge inside the pulse starts it again", "[parts]") {
    // Rise at 1 us, fall at 1.01 us, rise at 1.02 us: high from the last rise for the width.
    Bench bench({{1_us, One}, {1010_ns, Zero}, {1020_ns, One}}, Zero);
    Simulator sim(bench.circuit);
    sim.powerOn(1);
    sim.runUntil(1015_ns);
    CHECK(sim.value(bench.b) == Zero);
    sim.runUntil(1020_ns + kWidth + kCouplingDelay - 1);
    CHECK(sim.value(bench.b) == One);
    sim.runUntil(1020_ns + kWidth + kCouplingDelay);
    CHECK(sim.value(bench.b) == Zero);
}

TEST_CASE("capacitor: the builder couples a driven line into a pulled one", "[parts]") {
    // The 1 MHz clock rises once in the first 2 us (it starts high): 22.09 ns, seen to the ns.
    const auto loaded =
        Load(CouplingNetlist("value=47p", "part R1 R value=470\nnet B R1.1\nnet GND R1.2\n"));
    const Time high = HighTime(*loaded);
    CHECK(high >= 22'000);
    CHECK(high <= 23'000);
    // Either way round: C1's pins swapped.
    auto text = CouplingNetlist("value=47p", "part R1 R value=470\nnet B R1.1\nnet GND R1.2\n");
    text.replace(text.find("X1.OUT C1.1"), 11, "X1.OUT C1.2");
    text.replace(text.find("net B C1.2"), 10, "net B C1.1");
    CHECK(HighTime(*Load(text)) == high);
    // The smallest pull sets the width: 1k and 470 in parallel act as 470.
    const auto two = Load(CouplingNetlist(
        "value=47p",
        "part R1 R value=470\npart R2 R value=1k\nnet B R1.1 R2.1\nnet GND R1.2 R2.2\n"));
    CHECK(HighTime(*two) == high);
}

TEST_CASE("capacitor: anywhere else it adds nothing", "[parts]") {
    // No pull: B only floats.
    {
        const auto loaded = Load(CouplingNetlist("value=47p", ""));
        Simulator sim(loaded->circuit);
        sim.powerOn(1);
        for (Time t = 100'000; t <= 2'000'000; t += 100'000) {
            sim.runUntil(t);
            CHECK(sim.isFloating(loaded->nets.at("B")));
        }
    }
    // Pulls on both sides: A is also pulled, so B reads its pull only.
    CHECK(HighTime(*Load(CouplingNetlist("value=47p", "part R1 R value=470\npart R2 R value=10k\n"
                                                      "net B R1.1\nnet GND R1.2\nnet A R2.1\n"
                                                      "net VCC R2.2\n"))) == 0);
    // Across the rails (decoupling) and from a rail to a pulled line.
    Load("part C1 C value=100n\npart J1 POWER\nnet VCC C1.1\nnet GND C1.2\n");
    CHECK(HighTime(*Load("part C1 C value=47p\npart R1 R value=470\npart U1 74LS04\n"
                         "net VCC C1.1\nnet B C1.2 R1.1 U1.1A\nnet GND R1.2 U1.2A U1.3A U1.4A "
                         "U1.5A U1.6A\n")) == 0);
}

TEST_CASE("capacitor: builder errors", "[parts]") {
    using Lines = std::vector<std::string>;
    CHECK(Errors("part C1 C\nnet A C1.1\nnet B C1.2\n") ==
          Lines{"1: C1: a capacitor needs value= (farads)"});
    CHECK(Errors("part C1 C value=47\nnet A C1.1\nnet B C1.2\n") ==
          Lines{"1: C1: bad capacitance '47': expected e.g. 47p, 100n, 4n7 or 1u"});
    CHECK(Errors("part C1 C value=47p\nnet A C1.1\n") ==
          Lines{"1: C1: a capacitor needs both pins connected"});
    CHECK(Errors("part C1 C value=47p\nnet A C1.1 C1.2\n") == Lines{"1: C1: both ends on net A"});
    CHECK(Errors("part C1 C value=47p farads=1\nnet A C1.1\nnet B C1.2\n") ==
          Lines{"1: C1: unknown parameter 'farads'"});
    CHECK(Errors("part C1 C value=47p\nnet A C1.3\n") ==
          Lines{"1: C1: a capacitor needs both pins connected", "2: C1 (C) has no pin 3"});
}
