#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../machine/test_points.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

using Recorded = std::map<std::string, std::vector<Sample>>;

constexpr Time kStep = 65'536_us;         // STEP period: 2^16 CLK cycles
constexpr std::size_t kFirstSettled = 9;  // index of STEP rise 10: 9 rises settle the chaser

Machine FaultFree() {
    auto machine = LoadNetlistMachine(std::filesystem::path(BW_NETLIST_DIR) / "chaser.bwn");
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

Machine LoadPuzzle(const std::string& name) {
    auto machine = LoadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (name + ".bwp"));
    REQUIRE(machine.has_value());
    return std::move(*machine);
}

// "S3" → "U5.QC", "STEP" → "U4.QD", "SER" → "U6.Y", else `name` is already "REF.PIN".
std::string PinOf(const std::string& name) {
    if (name == "STEP") {
        return "U4.QD";
    }
    if (name == "SER") {
        return "U6.Y";
    }
    if (name.size() == 2 && name[0] == 'S') {
        return std::string("U5.Q") + static_cast<char>('A' + (name[1] - '1'));
    }
    return name;
}

// Samples of each named Net from a seeded power-on of `machine` to `end`.
Recorded Record(const Machine& machine, Time end, const std::vector<std::string>& names) {
    auto built = machine.build();
    REQUIRE(built.has_value());
    const LoadedCircuit& loaded = **built;
    Simulator sim((*built)->circuit);
    sim.powerOn(1);
    std::map<std::string, NetId> nets;
    for (const std::string& name : names) {
        const std::string pin = PinOf(name);
        const std::size_t dot = pin.find('.');
        const auto net = PinNet(loaded, pin.substr(0, dot), pin.substr(dot + 1));
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

std::vector<Time> Rises(const std::vector<Sample>& samples) {
    std::vector<Time> rises;
    for (const Sample& s : samples) {
        if (s.value == One && s.time > 0) {
            rises.push_back(s.time);
        }
    }
    return rises;
}

// Value of `samples` at `t` (last Sample at or before `t`).
LogicValue At(const std::vector<Sample>& samples, Time t) {
    LogicValue value = X;
    for (const Sample& s : samples) {
        if (s.time > t) {
            break;
        }
        value = s.value;
    }
    return value;
}

std::vector<std::string> ChaserNames(std::vector<std::string> extra = {}) {
    std::vector<std::string> names{"STEP"};
    for (char n = '1'; n <= '8'; ++n) {
        names.push_back(std::string("S") + n);
    }
    names.insert(names.end(), extra.begin(), extra.end());
    return names;
}

// The stages S1..S8 read 1 µs before each of the STEP rises 10..17: one string of '0'/'1'/'x'
// per rise.
std::vector<std::string> Stages(const Recorded& recorded) {
    const std::vector<Time> rises = Rises(recorded.at("STEP"));
    REQUIRE(rises.size() >= kFirstSettled + 8);
    std::vector<std::string> stages;
    for (std::size_t r = kFirstSettled; r < kFirstSettled + 8; ++r) {
        std::string text;
        for (char n = '1'; n <= '8'; ++n) {
            text += ToChar(At(recorded.at(std::string("S") + n), rises[r] - 1_us));
        }
        stages.push_back(text);
    }
    return stages;
}

// Exactly one '0' per entry, one stage further on (mod 8) at each rise.
void CheckRunning(const std::vector<std::string>& stages) {
    std::size_t previous = std::string::npos;
    for (const std::string& entry : stages) {
        INFO(entry);
        REQUIRE(std::ranges::count(entry, '0') == 1);
        REQUIRE(std::ranges::count(entry, '1') == 7);
        const std::size_t low = entry.find('0');
        if (previous != std::string::npos) {
            CHECK(low == (previous + 1) % 8);
        }
        previous = low;
    }
}

}  // namespace

TEST_CASE("chaser: fault-free light runs", "[machines][slow]") {
    const Recorded recorded = Record(FaultFree(), 18 * kStep, ChaserNames());
    const std::vector<Time> rises = Rises(recorded.at("STEP"));
    REQUIRE(rises.size() >= 17);
    for (std::size_t i = 1; i < rises.size(); ++i) {
        CHECK(rises[i] - rises[i - 1] == kStep);
    }
    CheckRunning(Stages(recorded));
}

TEST_CASE("chaser: Puzzles have a title and a brief", "[machines]") {
    for (const char* name : {"gap", "stuck_light", "dark_chaser", "twins"}) {
        INFO(name);
        const auto puzzle =
            ReadPuzzleFile(std::filesystem::path(BW_PUZZLE_DIR) / (std::string(name) + ".bwp"));
        REQUIRE(puzzle.has_value());
        CHECK(puzzle->machine == "chaser");
        CHECK_FALSE(puzzle->title.empty());
        CHECK_FALSE(puzzle->brief.empty());
    }
}

TEST_CASE("chaser: gap", "[machines][slow]") {
    Machine machine = LoadPuzzle("gap");
    const Recorded recorded = Record(machine, 18 * kStep, ChaserNames({"D5.A"}));
    for (const Sample& s : recorded.at("D5.A")) {
        CHECK(s.floating);
    }
    const std::vector<std::string> stages = Stages(recorded);
    CHECK(std::ranges::count_if(stages, [](const std::string& e) { return e[4] == '0'; }) == 1);

    REQUIRE(machine.mend(LedA5Crack().layer, LedA5Crack().at));
    for (const Sample& s : Record(machine, 1_ms, {"D5.A"}).at("D5.A")) {
        CHECK_FALSE(s.floating);
    }
}

TEST_CASE("chaser: stuck_light", "[machines][slow]") {
    Machine machine = LoadPuzzle("stuck_light");
    for (const std::string& entry : Stages(Record(machine, 18 * kStep, ChaserNames()))) {
        CHECK(entry == "11101111");
    }

    REQUIRE(machine.swap("U5"));
    CheckRunning(Stages(Record(machine, 18 * kStep, ChaserNames())));
}

TEST_CASE("chaser: dark_chaser", "[machines][slow]") {
    Machine machine = LoadPuzzle("dark_chaser");
    const Recorded recorded = Record(machine, 18 * kStep, ChaserNames({"U5.A"}));
    for (const Sample& s : recorded.at("U5.A")) {
        CHECK(s.floating);
        CHECK(s.value == One);
    }
    for (const std::string& entry : Stages(recorded)) {
        CHECK(entry == "11111111");
    }

    REQUIRE(machine.mend(SerCrack().layer, SerCrack().at));
    CheckRunning(Stages(Record(machine, 18 * kStep, ChaserNames())));
}

TEST_CASE("chaser: twins", "[machines][slow]") {
    Machine machine = LoadPuzzle("twins");
    const std::vector<std::string> stages = Stages(Record(machine, 18 * kStep, ChaserNames()));
    std::vector<std::size_t> low;
    for (std::size_t i = 0; i < stages.size(); ++i) {
        INFO(stages[i]);
        CHECK(stages[i][0] == stages[i][1]);
        if (stages[i][0] == '0') {
            low.push_back(i);
        }
    }
    REQUIRE(low.size() == 2);
    CHECK(((low[0] + 1) % 8 == low[1] || (low[1] + 1) % 8 == low[0]));

    REQUIRE(machine.removeBridge({"U5", "3"}, {"U5", "4"}));
    CheckRunning(Stages(Record(machine, 18 * kStep, ChaserNames())));
}
