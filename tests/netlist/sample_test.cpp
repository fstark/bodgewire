#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "parts/catalogue.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

std::unique_ptr<LoadedCircuit> LoadCounter() {
    auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "counter.bwn");
    REQUIRE(result.has_value());
    return std::move(*result);
}

unsigned Count(const LoadedCircuit& loaded, const Simulator& sim) {
    unsigned value = 0;
    for (unsigned bit = 0; bit < 8; ++bit) {
        const LogicValue v = sim.value(loaded.nets.at("C" + std::to_string(bit)));
        REQUIRE(v != X);
        value |= (v == One ? 1u : 0u) << bit;
    }
    return value;
}

// Same circuit as counter.bwn through the C++ API: same Component and Net creation order.
struct ApiCounter {
    Circuit circuit;
    std::map<std::string, NetId> nets;

    ApiCounter() {
        for (const char* name : {"GND", "VCC"}) {
            nets[name] = circuit.addNet(name);
        }
        circuit.tie(nets["GND"], Zero, DriveStrength::Supply);
        circuit.tie(nets["VCC"], One, DriveStrength::Supply);

        const std::vector<PartParam> freq{{"freq", "1MHz"}};
        Component& x1 = *FindPart("OSC")->create(circuit, "X1", freq).value();
        Component& u1 = *FindPart("74LS161")->create(circuit, "U1", {}).value();
        Component& u2 = *FindPart("74LS161")->create(circuit, "U2", {}).value();
        Component& u3 = *FindPart("74LS74")->create(circuit, "U3", {}).value();
        Component& u4 = *FindPart("74LS00")->create(circuit, "U4", {}).value();

        for (const char* name : {"CLK", "RESET_N", "C0", "C1", "C2", "C3", "CARRY", "C4", "C5",
                                 "C6", "C7", "DIV", "DIV_N", "TOP_N"}) {
            nets[name] = circuit.addNet(name);
        }
        circuit.tie(nets["RESET_N"], One, DriveStrength::Weak);  // R1

        for (Component* chip : {&x1, &u1, &u2, &u3, &u4}) {
            circuit.connect(*chip, "GND", nets["GND"]);
            circuit.connect(*chip, "VCC", nets["VCC"]);
        }
        circuit.connect(x1, "OUT", nets["CLK"]);
        for (Component* counter : {&u1, &u2}) {
            circuit.connect(*counter, "CLK", nets["CLK"]);
            circuit.connect(*counter, "CLR_N", nets["RESET_N"]);
            circuit.connect(*counter, "ENP", nets["VCC"]);
            circuit.connect(*counter, "LOAD_N", nets["VCC"]);
            for (const char* data : {"A", "B", "C", "D"}) {
                circuit.connect(*counter, data, nets["GND"]);
            }
        }
        circuit.connect(u1, "ENT", nets["VCC"]);
        circuit.connect(u1, "RCO", nets["CARRY"]);
        circuit.connect(u2, "ENT", nets["CARRY"]);
        const char* outputs[] = {"QA", "QB", "QC", "QD"};
        for (int bit = 0; bit < 4; ++bit) {
            circuit.connect(u1, outputs[bit], nets["C" + std::to_string(bit)]);
            circuit.connect(u2, outputs[bit], nets["C" + std::to_string(bit + 4)]);
        }
        circuit.connect(u3, "1CLK", nets["C7"]);
        circuit.connect(u3, "1D", nets["DIV_N"]);
        circuit.connect(u3, "1Q", nets["DIV"]);
        circuit.connect(u3, "1Q_N", nets["DIV_N"]);
        for (const char* pin : {"1PRE_N", "1CLR_N", "2PRE_N", "2CLR_N"}) {
            circuit.connect(u3, pin, nets["VCC"]);
        }
        circuit.connect(u3, "2CLK", nets["GND"]);
        circuit.connect(u3, "2D", nets["GND"]);
        circuit.connect(u4, "1A", nets["C6"]);
        circuit.connect(u4, "1B", nets["C7"]);
        circuit.connect(u4, "1Y", nets["TOP_N"]);
        for (const char* pin : {"2A", "2B", "3A", "3B", "4A", "4B"}) {
            circuit.connect(u4, pin, nets["GND"]);
        }
        circuit.finalize();
    }
};

}  // namespace

TEST_CASE("sample: counter.bwn loads without warnings", "[netlist]") {
    const auto loaded = LoadCounter();
    CHECK(loaded->warnings.empty());
    CHECK(loaded->components.size() == 5);
}

TEST_CASE("sample: counter counts", "[netlist]") {
    const auto loaded = LoadCounter();
    Simulator sim(loaded->circuit);
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        sim.powerOn(seed);
        sim.runUntil(2'250_ns);  // between clock edges (multiples of 500 ns)
        const unsigned n0 = Count(*loaded, sim);
        sim.runFor(100_us);
        CHECK(Count(*loaded, sim) == (n0 + 100) % 256);
    }
}

TEST_CASE("sample: DIV toggles on each rising C7", "[netlist]") {
    const auto loaded = LoadCounter();
    const NetId c7 = loaded->nets.at("C7");
    const NetId div = loaded->nets.at("DIV");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    sim.record(c7);
    sim.record(div);
    sim.runUntil(1'100_us);

    std::vector<Time> rises;
    for (const Sample& s : sim.recorder().samples(c7)) {
        if (s.value == One && s.time > 0) {
            rises.push_back(s.time);
        }
    }
    const auto changes = sim.recorder().samples(div).subspan(1);
    REQUIRE(rises.size() >= 3);
    CHECK(changes.size() == rises.size());
    for (const Sample& change : changes) {
        Time latestRise = -1;
        for (const Time rise : rises) {
            if (rise < change.time) {
                latestRise = rise;
            }
        }
        const Time delay = change.time - latestRise;
        CHECK((delay == 13_ns || delay == 25_ns));
    }
}

TEST_CASE("sample: TOP_N", "[netlist]") {
    const auto loaded = LoadCounter();
    const NetId c6 = loaded->nets.at("C6");
    const NetId c7 = loaded->nets.at("C7");
    const NetId topN = loaded->nets.at("TOP_N");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    int lows = 0;
    for (Time t = 2'250_ns; t < 600_us; t += 1_us) {
        sim.runUntil(t);
        const bool top = sim.value(c6) == One && sim.value(c7) == One;
        CHECK(sim.value(topN) == (top ? Zero : One));
        lows += top ? 1 : 0;
    }
    CHECK(lows > 0);
}

TEST_CASE("sample: same as API-built circuit", "[netlist]") {
    const auto loaded = LoadCounter();
    ApiCounter api;
    REQUIRE(api.circuit.netCount() == loaded->circuit.netCount());

    Simulator fromFile(loaded->circuit);
    Simulator fromApi(api.circuit);
    fromFile.powerOn(7);
    fromApi.powerOn(7);
    for (const auto& [name, net] : loaded->nets) {
        fromFile.record(net);
        fromApi.record(api.nets.at(name));
    }
    fromFile.runUntil(600_us);
    fromApi.runUntil(600_us);
    for (const auto& [name, net] : loaded->nets) {
        INFO(name);
        const auto a = fromFile.recorder().samples(net);
        const auto b = fromApi.recorder().samples(api.nets.at(name));
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            CHECK(a[i] == b[i]);
        }
    }
    CHECK(fromFile.stats().appliedEvents == fromApi.stats().appliedEvents);
}
