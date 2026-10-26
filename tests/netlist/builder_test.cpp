#include "netlist/builder.hpp"

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "sim/simulator.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

using enum LogicValue;

// Errors of `text` as "line: message".
std::vector<std::string> Errors(std::string_view text) {
    const auto result = LoadNetlist(text, "t.bwn");
    REQUIRE_FALSE(result.has_value());
    std::vector<std::string> errors;
    for (const Diagnostic& d : result.error()) {
        CHECK(d.file == "t.bwn");
        CHECK(d.severity == Severity::Error);
        errors.push_back(std::to_string(d.line) + ": " + d.message);
    }
    return errors;
}

using Lines = std::vector<std::string>;

NetId NetOfPin(const LoadedCircuit& loaded, std::string_view reference, PinIndex pin) {
    const Component& component = *loaded.components.find(reference)->second;
    return loaded.circuit.netOf(PinId{ToIndex(component.firstPin()) + pin});
}

}  // namespace

TEST_CASE("builder: duplicate reference", "[netlist]") {
    CHECK(Errors("part U1 74LS00\n\n\npart U1 74LS04\n") ==
          Lines{"4: U1 already placed at line 1"});
}

TEST_CASE("builder: unknown part", "[netlist]") {
    CHECK(Errors("part U1 74LS999\nnet A U1.1\n") == Lines{"1: unknown part '74LS999'"});
}

TEST_CASE("builder: factory error", "[netlist]") {
    CHECK(Errors("part U1 74LS00 speed=fast\n") == Lines{"1: U1: 74LS00 takes no parameters"});
}

TEST_CASE("builder: unknown reference", "[netlist]") {
    CHECK(Errors("part U1 74LS00\nnet A U1.1 U9.1\n") == Lines{"2: unknown reference 'U9'"});
}

TEST_CASE("builder: unknown pin", "[netlist]") {
    CHECK(Errors("part U3 74LS00\nnet A U3.15\nnet B U3.XYZ\nnet C U3.0\n") ==
          Lines{"2: U3 (74LS00) has no pin 15", "3: U3 (74LS00) has no pin XYZ",
                "4: U3 (74LS00) has no pin 0"});
}

TEST_CASE("builder: no-connect", "[netlist]") {
    CHECK(Errors("part U5 74LS20\nnet A U5.3\n") ==
          Lines{"2: U5.3 (NC3) is not connected inside 74LS20"});
}

TEST_CASE("builder: power pin", "[netlist]") {
    CHECK(Errors("part U1 74LS00\nnet A U1.7\nnet GND U1.VCC\n") ==
          Lines{"2: U1.7 (GND) can only be on net GND", "3: U1.VCC (VCC) can only be on net VCC"});
}

TEST_CASE("builder: pin on two nets", "[netlist]") {
    CHECK(Errors("part U1 74LS00\nnet A U1.3\nnet B U1.3\n") ==
          Lines{"3: U1.3 (1Y) is on nets A and B"});
}

TEST_CASE("builder: parameter errors", "[netlist]") {
    CHECK(Errors("part X1 OSC\n") == Lines{"1: X1: OSC needs freq=<frequency>"});
    CHECK(Errors("part X1 OSC foo=1\n") == Lines{"1: X1: OSC: unknown parameter 'foo'"});
    CHECK(Errors("part X1 OSC freq=10GHz\n") ==
          Lines{"1: X1: bad frequency '10GHz': expected a number then Hz, kHz or MHz, e.g. "
                "14.31818MHz"});
}

TEST_CASE("builder: reports all errors", "[netlist]") {
    CHECK(Errors("net A U1.1 U9.1\n"
                 "part U1 74LS00\n"
                 "part U2 74LS999\n"
                 "part U1 74LS04\n"
                 "net B U1.15 U1.7\n") ==
          Lines{"1: unknown reference 'U9'", "3: unknown part '74LS999'",
                "4: U1 already placed at line 2", "5: U1 (74LS00) has no pin 15",
                "5: U1.7 (GND) can only be on net GND"});
}

TEST_CASE("builder: syntax errors stop before building", "[netlist]") {
    CHECK(Errors("part U1 74LS00\nwire A\n") == Lines{"2: unknown statement 'wire'"});
}

TEST_CASE("builder: pin by number and by name are the same pin", "[netlist]") {
    CHECK(Errors("part U1 74LS00\nnet A U1.3\nnet B U1.1Y\n") ==
          Lines{"3: U1.1Y (1Y) is on nets A and B"});
    CHECK(LoadNetlist("part U1 74LS00\nnet A U1.3 U1.1Y\n", "t.bwn").has_value());
}

TEST_CASE("builder: net spans several lines", "[netlist]") {
    const auto result =
        LoadNetlist("part U1 74LS00\nnet A U1.1\nnet B U1.3\nnet A U1.2\n", "t.bwn");
    REQUIRE(result.has_value());
    const LoadedCircuit& loaded = **result;
    const NetId a = loaded.nets.at("A");
    CHECK(NetOfPin(loaded, "U1", 0) == a);
    CHECK(NetOfPin(loaded, "U1", 1) == a);
    CHECK(NetOfPin(loaded, "U1", 2) == loaded.nets.at("B"));
}

TEST_CASE("builder: power pins connect implicitly", "[netlist]") {
    for (const char* text : {"part U1 74LS00\n", "part U1 74LS00\nnet VCC U1.14\n"}) {
        const auto result = LoadNetlist(text, "t.bwn");
        REQUIRE(result.has_value());
        const LoadedCircuit& loaded = **result;
        CHECK(NetOfPin(loaded, "U1", 13) == loaded.nets.at("VCC"));
        CHECK(NetOfPin(loaded, "U1", 6) == loaded.nets.at("GND"));
    }
}

TEST_CASE("builder: GND and VCC exist and are supply", "[netlist]") {
    const auto result = LoadNetlist("", "t.bwn");
    REQUIRE(result.has_value());
    LoadedCircuit& loaded = **result;
    const NetId gnd = loaded.nets.at("GND");
    const NetId vcc = loaded.nets.at("VCC");
    CHECK(loaded.circuit.tieOf(gnd).strength == DriveStrength::Supply);
    CHECK(loaded.circuit.tieOf(vcc).strength == DriveStrength::Supply);
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    CHECK(sim.value(gnd) == Zero);
    CHECK(sim.value(vcc) == One);
}

TEST_CASE("builder: simulates", "[netlist]") {
    const auto result = LoadNetlist("part U1 74LS04\nnet GND U1.1A\nnet OUT U1.1Y\n", "t.bwn");
    REQUIRE(result.has_value());
    LoadedCircuit& loaded = **result;
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    CHECK(sim.value(loaded.nets.at("OUT")) == One);
}
