#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "devices/led_glow.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

using Recorded = std::map<std::string, std::vector<Sample>>;

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

Machine FaultFree() {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "counter.bwn");
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

// "C7" → "U2.QD", "CARRY3" → "U3.RCO", else `name` is already "REF.PIN".
std::string PinOf(const std::string& name) {
    if (name.starts_with("CARRY")) {
        return "U" + name.substr(5) + ".RCO";
    }
    if (name.starts_with("C")) {
        const int bit = std::stoi(name.substr(1));
        return "U" + std::to_string(bit / 4 + 1) + ".Q" + static_cast<char>('A' + bit % 4);
    }
    return name;
}

// Samples of each named Net (or REF.PIN pad) from a seeded power-on to `end`.
Recorded Record(const Machine& machine, Time end, const std::vector<std::string>& names) {
    auto loaded = machine.build();
    REQUIRE(loaded.has_value());
    Simulator sim((*loaded)->circuit);
    sim.powerOn(1);
    std::map<std::string, NetId> nets;
    for (const std::string& name : names) {
        const std::string pin = PinOf(name);
        const std::size_t dot = pin.find('.');
        const auto net = PinNet(**loaded, pin.substr(0, dot), pin.substr(dot + 1));
        REQUIRE(net.has_value());
        nets[name] = *net;
        sim.record(*net);
    }
    sim.runUntil(end);
    Recorded recorded;
    for (const auto& [name, net] : nets) {
        const auto samples = sim.recorder().samples(net);
        recorded[name].assign(samples.begin(), samples.end());
    }
    return recorded;
}

std::size_t Changes(const std::vector<Sample>& samples) { return samples.size() - 1; }

std::vector<Time> Rises(const std::vector<Sample>& samples) {
    std::vector<Time> rises;
    for (const Sample& s : samples) {
        if (s.value == One && s.time > 0) {
            rises.push_back(s.time);
        }
    }
    return rises;
}

// Brightness of every whole LedGlow window up to `end`, from D1's anode and cathode Samples.
std::vector<float> Glow(const std::vector<Sample>& anode, const std::vector<Sample>& cathode,
                        Time end) {
    LedGlow glow;
    std::size_t a = 0;
    std::size_t k = 0;
    std::vector<float> windows;
    for (Time windowEnd = kLedWindow; windowEnd <= end; windowEnd += kLedWindow) {
        while (true) {
            const bool hasA = a < anode.size() && anode[a].time < windowEnd;
            const bool hasK = k < cathode.size() && cathode[k].time < windowEnd;
            if (hasA && (!hasK || anode[a].time <= cathode[k].time)) {
                glow.setAnode(anode[a++]);
            } else if (hasK) {
                glow.setCathode(cathode[k++]);
            } else {
                break;
            }
        }
        glow.advanceTo(windowEnd);
        windows.push_back(glow.brightness());
    }
    return windows;
}

}  // namespace

TEST_CASE("counter: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : {"dark_led", "frozen_led", "too_fast"}) {
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "counter");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("counter: fault-free bits and carries run", "[machines]") {
    std::vector<std::string> names{"CARRY1", "CARRY2"};
    for (int bit = 0; bit < 12; ++bit) {
        names.push_back("C" + std::to_string(bit));
    }
    const Recorded recorded = Record(FaultFree(), 10_ms, names);
    for (int bit = 0; bit < 12; ++bit) {
        INFO("C" << bit);
        CHECK(Changes(recorded.at("C" + std::to_string(bit))) > 0);
    }
    CHECK_FALSE(Rises(recorded.at("CARRY1")).empty());
    CHECK_FALSE(Rises(recorded.at("CARRY2")).empty());
}

TEST_CASE("counter: frozen_led", "[machines]") {
    std::vector<std::string> names{"CARRY3"};
    for (int bit = 12; bit < 20; ++bit) {
        names.push_back("C" + std::to_string(bit));
    }
    Machine machine = LoadPuzzle("frozen_led");
    const Recorded recorded = Record(machine, 10_ms, names);
    for (const Sample& s : recorded.at("CARRY3")) {
        CHECK(s.value == Zero);
    }
    for (int bit = 12; bit < 20; ++bit) {
        INFO("C" << bit);
        CHECK(Changes(recorded.at("C" + std::to_string(bit))) == 0);
    }

    REQUIRE(machine.swap("U3"));
    CHECK_FALSE(Rises(Record(machine, 10_ms, {"CARRY3"}).at("CARRY3")).empty());
}

TEST_CASE("counter: too_fast", "[machines]") {
    Machine machine = LoadPuzzle("too_fast");
    CHECK(Changes(Record(machine, 70_ms, {"C19"}).at("C19")) > 0);
    const Recorded led = Record(machine, 140_ms, {"D1.A", "D1.K"});
    const std::vector<float> windows = Glow(led.at("D1.A"), led.at("D1.K"), 140_ms);
    REQUIRE(windows.size() == 7);
    CHECK(std::ranges::any_of(windows, [&](float w) { return w != windows[0]; }));

    REQUIRE(machine.swap("U1"));
    const std::vector<Time> rises = Rises(Record(machine, 100_us, {"CARRY1"}).at("CARRY1"));
    REQUIRE(rises.size() >= 5);
    for (std::size_t i = 1; i < rises.size(); ++i) {
        CHECK(rises[i] - rises[i - 1] == 16_us);
    }
    CHECK(Changes(Record(machine, 70_ms, {"C19"}).at("C19")) <= 1);
}

TEST_CASE("counter: dark_led", "[machines]") {
    Machine machine = LoadPuzzle("dark_led");
    const Recorded led = Record(machine, 40_ms, {"D1.A", "D1.K"});
    for (const Sample& s : led.at("D1.K")) {
        CHECK(s.floating);
    }
    CHECK(Glow(led.at("D1.A"), led.at("D1.K"), 40_ms) == std::vector<float>{0, 0});

    REQUIRE(machine.mend(Layer::Front, kC19Crack));
    auto loaded = machine.build();
    REQUIRE(loaded.has_value());
    CHECK(PinNet(**loaded, "D1", "K") == PinNet(**loaded, "U5", "QD"));
}

TEST_CASE("counter: LED blinks once a second", "[machines][slow]") {
    const Recorded led = Record(FaultFree(), 1'100_ms, {"D1.A", "D1.K"});
    const std::vector<float> windows = Glow(led.at("D1.A"), led.at("D1.K"), 1'100_ms);
    CHECK(std::ranges::find(windows, 0.0f) != windows.end());
    CHECK(std::ranges::find(windows, 1.0f) != windows.end());
}
