#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../machine/test_points.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "parts/ls47.hpp"
#include "parts/paddle.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "knob.bwn");
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Circuit built from the Board's Copper, with no Faults.
std::unique_ptr<LoadedCircuit> FaultFree() {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "knob.bwn");
    REQUIRE(machine.has_value());
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// Lit segments of one digit, bit k for segment 'A' + k.
unsigned Segments(const LoadedCircuit& loaded, const Simulator& sim, std::string_view digit) {
    unsigned lit = 0;
    for (int k = 0; k < 7; ++k) {
        const NetId net = *PinNet(loaded, digit, std::string(1, static_cast<char>('A' + k)));
        if (sim.value(net) == Zero && !sim.isFloating(net)) {
            lit |= 1u << k;
        }
    }
    return lit;
}

// One digit's shape as text: '0'..'9' as the 74LS47 draws them, ' ' dark, '?' anything else.
char DigitAt(const LoadedCircuit& loaded, const Simulator& sim, std::string_view digit) {
    const unsigned lit = Segments(loaded, sim, digit);
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

// A powered knob meter build with its knob.
class Meter {
public:
    Meter(LoadedCircuit& loaded, std::uint64_t seed, double knob)
        : loaded_(loaded), sim_(loaded.circuit) {
        paddle_ = PaddleOf(loaded_, "J2");
        REQUIRE(paddle_ != nullptr);
        paddle_->setKnob(knob);
        sim_.powerOn(seed);
    }

    Simulator& sim() { return sim_; }
    void runUntil(Time t) { sim_.runUntil(t); }
    void turn(double knob) { paddle_->setKnob(knob); }

    // "dd" from DS1, DS2.
    std::string display() const {
        return {DigitAt(loaded_, sim_, "DS1"), DigitAt(loaded_, sim_, "DS2")};
    }
    // Both digits' segment masks, DS1 in the high bits.
    unsigned segments() const {
        return Segments(loaded_, sim_, "DS1") << 7 | Segments(loaded_, sim_, "DS2");
    }

private:
    LoadedCircuit& loaded_;
    Simulator sim_;
    Paddle* paddle_ = nullptr;
};

// Rising and falling times of a recorded Net, in order.
std::vector<Sample> Edges(const Simulator& sim, NetId net) {
    const auto samples = sim.recorder().samples(net);
    return {samples.begin() + 1, samples.end()};
}

}  // namespace

TEST_CASE("knob: netlist loads without warnings", "[machines]") {
    CHECK(DesignCircuit()->warnings.empty());
}

TEST_CASE("knob: reads the knob", "[machines][slow]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    auto loaded = FaultFree();
    Meter meter(*loaded, seed, 0.0);
    meter.runUntil(250_ms);
    CHECK(meter.display() == "00");
    meter.runUntil(350_ms);
    CHECK(meter.display() == "00");
    Time t = 350_ms;
    for (const auto& [knob, reading] : std::vector<std::pair<double, std::string>>{
             {0.2, "20"}, {0.5, "50"}, {0.8, "79"}, {1.0, "99"}}) {
        INFO("knob " << knob);
        meter.turn(knob);
        t += 200_ms;
        meter.runUntil(t);
        CHECK(meter.display() == reading);
    }
}

TEST_CASE("knob: follows a turn", "[machines]") {
    auto loaded = FaultFree();
    Meter meter(*loaded, 4, 0.3);
    meter.runUntil(250_ms);
    CHECK(meter.display() == "30");
    meter.turn(0.6);
    meter.runUntil(450_ms);
    CHECK(meter.display() == "59");
}

TEST_CASE("knob: one measuring cycle", "[machines]") {
    auto loaded = FaultFree();
    Meter meter(*loaded, 1, 0.5);
    const NetId trig = loaded->nets.at("TRIG_N");
    const NetId pulse = loaded->nets.at("PULSE");
    const NetId latch = loaded->nets.at("LATCH");
    const NetId tb1 = loaded->nets.at("TB1");
    const NetId tb2 = loaded->nets.at("TB2");
    for (const NetId net : {trig, pulse, latch, tb1, tb2}) {
        meter.sim().record(net);
    }
    meter.runUntil(320_ms);

    std::vector<Time> falls;  // of TRIG_N
    for (const Sample& s : Edges(meter.sim(), trig)) {
        if (s.value == Zero) {
            falls.push_back(s.time);
        }
    }
    REQUIRE(falls.size() >= 3);
    for (std::size_t i = 1; i < falls.size(); ++i) {
        CHECK(falls[i] - falls[i - 1] == 100_ms);
    }
    const Time trigger = falls[1];

    std::vector<Sample> pulses;
    for (const Sample& s : Edges(meter.sim(), pulse)) {
        if (s.time > trigger && s.time < trigger + 100_ms) {
            pulses.push_back(s);
        }
    }
    REQUIRE(pulses.size() == 2);
    CHECK(pulses[0].value == One);
    CHECK(pulses[0].time == trigger + kPaddleDelay);
    CHECK(pulses[1].time - pulses[0].time == 30_us + 4950_us);

    std::vector<Sample> latches;
    for (const Sample& s : Edges(meter.sim(), latch)) {
        if (s.time > trigger && s.time < trigger + 100_ms && s.value == One) {
            latches.push_back(s);
        }
    }
    REQUIRE(latches.size() == 1);
    CHECK(latches[0].time > pulses[1].time);
    CHECK(latches[0].time < pulses[1].time + 100_ns);

    // TB1·TB2 high from 60 ms to 80 ms after the trigger, give or take the ripple.
    const auto level = [&](NetId net, Time at) {
        LogicValue value = X;
        for (const Sample& s : meter.sim().recorder().samples(net)) {
            if (s.time <= at) {
                value = s.value;
            }
        }
        return value;
    };
    for (const Time at : {trigger + 10_ms, trigger + 59_ms, trigger + 81_ms}) {
        CHECK_FALSE((level(tb1, at) == One && level(tb2, at) == One));
    }
    for (const Time at : {trigger + 61_ms, trigger + 79_ms}) {
        CHECK(level(tb1, at) == One);
        CHECK(level(tb2, at) == One);
    }
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

// The reading at 250 ms (two full cycles) with the knob at `knob` from power-on.
std::string ReadingAt(LoadedCircuit& loaded, double knob, std::uint64_t seed = 1) {
    Meter meter(loaded, seed, knob);
    meter.runUntil(250_ms);
    return meter.display();
}

}  // namespace

TEST_CASE("knob: Puzzles have a title and a brief", "[machines]") {
    for (const char* name :
         {"dead_knob", "runaway", "single_figures", "odd_tens", "four_to_seven"}) {
        INFO(name);
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "knob");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("knob: dead_knob", "[machines][slow]") {
    Machine machine = LoadPuzzle("dead_knob");
    CHECK(machine.power().unpowered == std::vector<std::string>{"J2"});
    {
        auto loaded = Build(machine);
        const NetId pulse = loaded->nets.at("PULSE");
        for (const std::uint64_t seed : {1, 2, 3}) {
            INFO("seed " << seed);
            Meter meter(*loaded, seed, 0.2);
            meter.runUntil(250_ms);
            const unsigned first = meter.segments();
            CHECK(meter.sim().isFloating(pulse));
            meter.turn(0.8);
            meter.runUntil(450_ms);
            CHECK(meter.segments() == first);
        }
    }
    REQUIRE(machine.mend(DeadKnobCrack().layer, DeadKnobCrack().at));
    CHECK(machine.power().unpowered.empty());
    auto loaded = Build(machine);
    Meter meter(*loaded, 1, 0.2);
    meter.runUntil(250_ms);
    CHECK(meter.display() == "20");
    meter.turn(0.8);
    meter.runUntil(450_ms);
    CHECK(meter.display() == "79");
}

namespace {

int Number(const std::string& display) {
    REQUIRE(display.size() == 2);
    REQUIRE(display[0] >= '0');
    REQUIRE(display[0] <= '9');
    REQUIRE(display[1] >= '0');
    REQUIRE(display[1] <= '9');
    return (display[0] - '0') * 10 + (display[1] - '0');
}

}  // namespace

TEST_CASE("knob: runaway", "[machines][slow]") {
    Machine machine = LoadPuzzle("runaway");
    {
        auto loaded = Build(machine);
        for (const std::uint64_t seed : {1, 2, 3}) {
            INFO("seed " << seed);
            Meter meter(*loaded, seed, 0.3);
            meter.runUntil(250_ms);
            const int first = Number(meter.display());
            meter.runUntil(350_ms);
            CHECK(Number(meter.display()) == (first + 30) % 100);
        }
    }
    REQUIRE(machine.swap("U5"));
    auto loaded = Build(machine);
    Meter meter(*loaded, 1, 0.3);
    meter.runUntil(250_ms);
    CHECK(meter.display() == "30");
    meter.runUntil(350_ms);
    CHECK(meter.display() == "30");
}

TEST_CASE("knob: single_figures", "[machines]") {
    Machine machine = LoadPuzzle("single_figures");
    {
        auto loaded = Build(machine);
        const NetId cka = *PinNet(*loaded, "U7", "CKA");
        REQUIRE(cka != *PinNet(*loaded, "U8", "QD"));
        REQUIRE(*PinNet(*loaded, "U10", "4D") == *PinNet(*loaded, "U8", "QD"));
        Meter meter(*loaded, 1, 0.47);
        meter.runUntil(250_ms);
        CHECK(meter.display() == "07");
        CHECK(meter.sim().isFloating(cka));
    }
    REQUIRE(machine.mend(SingleFiguresCrack().layer, SingleFiguresCrack().at));
    auto loaded = Build(machine);
    CHECK(ReadingAt(*loaded, 0.47) == "47");
}

TEST_CASE("knob: odd_tens", "[machines]") {
    Machine machine = LoadPuzzle("odd_tens");
    {
        auto loaded = Build(machine);
        CHECK(PinNet(*loaded, "U11", "A") != loaded->nets.at("L4"));  // the leg is off its pad
        CHECK(ReadingAt(*loaded, 0.0) == "10");
        CHECK(ReadingAt(*loaded, 0.25) == "35");
    }
    REQUIRE(machine.resolder({"U11", "7"}));
    auto loaded = Build(machine);
    CHECK(ReadingAt(*loaded, 0.0) == "00");
    CHECK(ReadingAt(*loaded, 0.25) == "25");
}

TEST_CASE("knob: four_to_seven", "[machines]") {
    Machine machine = LoadPuzzle("four_to_seven");
    {
        auto loaded = Build(machine);
        CHECK(*PinNet(*loaded, "U12", "C") == *PinNet(*loaded, "U12", "LT_N"));
        CHECK(ReadingAt(*loaded, 0.0) == "04");
        Meter meter(*loaded, 1, 0.09);
        meter.runUntil(250_ms);
        CHECK(DigitAt(*loaded, meter.sim(), "DS1") == '0');
        CHECK(Segments(*loaded, meter.sim(), "DS2") == Ls47Segments(13, true, true, true));
    }
    REQUIRE(machine.removeBridge({"U12", "2"}, {"U12", "3"}));
    auto loaded = Build(machine);
    CHECK(ReadingAt(*loaded, 0.0) == "00");
    CHECK(ReadingAt(*loaded, 0.09) == "09");
}
