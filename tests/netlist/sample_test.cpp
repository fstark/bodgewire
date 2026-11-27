#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "netlist/builder.hpp"
#include "parts/catalogue.hpp"
#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

std::unique_ptr<LoadedCircuit> LoadCounter() {
    auto result = LoadNetlistFile(test::NetlistPath("counter"));
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The 20-bit count from U1–U5's QA–QD (bit 0 U1.QA): their pins' Nets, named or their own.
unsigned Count(const LoadedCircuit& loaded, const Simulator& sim) {
    unsigned value = 0;
    for (unsigned bit = 0; bit < 20; ++bit) {
        const std::string chip = "U" + std::to_string(bit / 4 + 1);
        const std::string pin = std::string("Q") + static_cast<char>('A' + bit % 4);
        const auto net = PinNet(loaded, chip, pin);
        REQUIRE(net.has_value());
        const LogicValue v = sim.value(*net);
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
        std::vector<Component*> counters;
        for (const char* reference : {"U1", "U2", "U3", "U4", "U5"}) {
            counters.push_back(FindPart("74LS161")->create(circuit, reference, {}).value());
        }
        Component& d1 = *FindPart("LED")->create(circuit, "D1", {}).value();

        for (const char* name : {"CLK", "RESET_N"}) {
            nets[name] = circuit.addNet(name);
        }
        for (int chip = 0; chip < 4; ++chip) {
            const std::string carry = "CARRY" + std::to_string(chip + 1);
            nets[carry] = circuit.addNet(carry);
        }
        nets["C19"] = circuit.addNet("C19");  // the other outputs: their own Nets (finalize)
        nets["LED_A"] = circuit.addNet("LED_A");
        circuit.tie(nets["RESET_N"], One, DriveStrength::Weak);  // R1
        circuit.tie(nets["LED_A"], One, DriveStrength::Weak);    // R2

        circuit.connect(x1, "GND", nets["GND"]);
        circuit.connect(x1, "VCC", nets["VCC"]);
        circuit.connect(x1, "OUT", nets["CLK"]);
        const char* outputs[] = {"QA", "QB", "QC", "QD"};
        for (int chip = 0; chip < 5; ++chip) {
            Component& counter = *counters[chip];
            circuit.connect(counter, "GND", nets["GND"]);
            circuit.connect(counter, "VCC", nets["VCC"]);
            circuit.connect(counter, "CLK", nets["CLK"]);
            circuit.connect(counter, "CLR_N", nets["RESET_N"]);
            circuit.connect(counter, "ENP", nets["VCC"]);
            circuit.connect(counter, "LOAD_N", nets["VCC"]);
            for (const char* data : {"A", "B", "C", "D"}) {
                circuit.connect(counter, data, nets["GND"]);
            }
            circuit.connect(counter, "ENT",
                            nets[chip == 0 ? "VCC" : "CARRY" + std::to_string(chip)]);
            if (chip < 4) {
                circuit.connect(counter, "RCO", nets["CARRY" + std::to_string(chip + 1)]);
            }
            if (chip == 4) {
                circuit.connect(counter, outputs[3], nets["C19"]);
            }
        }
        circuit.connect(d1, "K", nets["C19"]);
        circuit.connect(d1, "A", nets["LED_A"]);
        circuit.finalize();
    }
};

}  // namespace

TEST_CASE("sample: counter.bwn loads without warnings", "[netlist]") {
    const auto loaded = LoadCounter();
    CHECK(loaded->warnings.empty());
    CHECK(loaded->components.size() == 8);  // X1, U1–U5, J1, D1
}

TEST_CASE("sample: counter counts", "[netlist]") {
    const auto loaded = LoadCounter();
    Simulator sim(loaded->circuit);
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        sim.powerOn(seed);
        sim.runUntil(2'250_ns);  // between clock edges (multiples of 500 ns)
        const unsigned n0 = Count(*loaded, sim);
        sim.runFor(100_us);
        CHECK(Count(*loaded, sim) == (n0 + 100) % (1u << 20));
    }
}

TEST_CASE("sample: carries pulse at their rates", "[netlist]") {
    const auto loaded = LoadCounter();
    const NetId carry1 = loaded->nets.at("CARRY1");
    const NetId carry2 = loaded->nets.at("CARRY2");
    Simulator sim(loaded->circuit);
    sim.powerOn(1);
    sim.record(carry1);
    sim.record(carry2);
    sim.runUntil(600_us);

    const auto rises = [&](NetId net) {
        std::vector<Time> times;
        for (const Sample& s : sim.recorder().samples(net)) {
            if (s.value == One && s.time > 0) {
                times.push_back(s.time);
            }
        }
        return times;
    };
    const std::vector<Time> rises1 = rises(carry1);
    REQUIRE(rises1.size() >= 36);
    for (std::size_t i = 1; i < rises1.size(); ++i) {
        CHECK(rises1[i] - rises1[i - 1] == 16_us);
    }
    const std::size_t rises2 = rises(carry2).size();
    CHECK(rises2 >= 2);
    CHECK(rises2 <= 3);
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
