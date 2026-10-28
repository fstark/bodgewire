#include "netlist/builder.hpp"

#include <filesystem>
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

TEST_CASE("builder: pull-up", "[netlist]") {
    // P: pull-up and an input only. Y: pull-up overridden by an output driving 0.
    const auto result = LoadNetlist("part R1 R value=4k7\n"
                                    "part R2 R\n"
                                    "part U1 74LS04\n"
                                    "net VCC R1.1 R2.2 U1.1A\n"
                                    "net P R1.2 U1.3A\n"
                                    "net Y R2.1 U1.1Y\n",
                                    "t.bwn");
    REQUIRE(result.has_value());
    LoadedCircuit& loaded = **result;
    CHECK(loaded.circuit.tieOf(loaded.nets.at("P")).strength == DriveStrength::Weak);
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    CHECK(sim.value(loaded.nets.at("P")) == One);
    CHECK_FALSE(sim.isFloating(loaded.nets.at("P")));
    CHECK(sim.value(loaded.nets.at("Y")) == Zero);
}

TEST_CASE("builder: pull-down", "[netlist]") {
    const auto result = LoadNetlist("part R1 R\n"
                                    "part R2 R\n"
                                    "part U1 74LS04\n"
                                    "net GND R1.1 R2.2 U1.1A\n"
                                    "net P R1.2 U1.3A\n"
                                    "net Y R2.1 U1.1Y\n",
                                    "t.bwn");
    REQUIRE(result.has_value());
    LoadedCircuit& loaded = **result;
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    CHECK(sim.value(loaded.nets.at("P")) == Zero);
    CHECK(sim.value(loaded.nets.at("Y")) == One);
}

TEST_CASE("builder: resistor errors", "[netlist]") {
    const std::string unsupported = "R1: only pull-up/pull-down resistors are supported (one pin "
                                    "on GND or VCC, the other on a signal net)";
    CHECK(Errors("part R1 R\nnet VCC R1.1\nnet GND R1.2\n") == Lines{"1: " + unsupported});
    CHECK(Errors("part R1 R\npart U1 74LS04\nnet A R1.1 U1.1A\nnet B R1.2 U1.3A\n") ==
          Lines{"1: " + unsupported});
    CHECK(Errors("part R1 R\nnet VCC R1.1\n") == Lines{"1: " + unsupported});
    CHECK(Errors("part R1 R\nnet VCC R1.1\nnet A R1.2 R1.3\n") == Lines{"3: R1 (R) has no pin 3"});
    CHECK(Errors("part R1 R ohms=10\nnet VCC R1.1\n") == Lines{"1: R1: unknown parameter 'ohms'"});
}

TEST_CASE("builder: unconnected input warning", "[netlist]") {
    const auto result = LoadNetlist("\npart U2 74LS04\nnet A U2.1 U2.3 U2.5 U2.9 U2.11\n", "t.bwn");
    REQUIRE(result.has_value());
    const auto& warnings = (*result)->warnings;
    REQUIRE(warnings.size() == 1);
    CHECK(Format(warnings[0]) == "t.bwn:2: warning: U2.13 (6A) is unconnected: floats high");
    CHECK((*result)->circuit.isFinalized());
}

TEST_CASE("builder: LoadNetlistFile missing file", "[netlist]") {
    const auto result = LoadNetlistFile("/nonexistent/dir/missing.bwn");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().size() == 1);
    CHECK(Format(result.error()[0]) == "missing.bwn: error: cannot read file");
}

TEST_CASE("builder: ConnectorNet finds the VIDEO pins", "[netlist]") {
    const auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(result.has_value());
    const auto video = ConnectorNet(**result, "VIDEO", "VIDEO");
    const auto syncN = ConnectorNet(**result, "VIDEO", "SYNC_N");
    REQUIRE(video.has_value());
    REQUIRE(syncN.has_value());
    CHECK(*video != *syncN);
    CHECK(*video == (*result)->nets.at("VIDEO"));
}

TEST_CASE("builder: ConnectorNet without the connector", "[netlist]") {
    const auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "counter.bwn");
    REQUIRE(result.has_value());
    CHECK_FALSE(ConnectorNet(**result, "VIDEO", "VIDEO").has_value());
}

TEST_CASE("builder: parts in netlist order with values", "[netlist]") {
    const auto result = LoadNetlist("part U2 74LS04\npart R1 R value=4k7\npart U1 74LS00\n"
                                    "net VCC R1.1\nnet A R1.2 U2.1\n",
                                    "t.bwn");
    REQUIRE(result.has_value());
    const auto& parts = (*result)->parts;
    REQUIRE(parts.size() == 3);
    CHECK(parts[0].reference == "U2");
    CHECK(parts[0].info->name == "74LS04");
    CHECK(parts[0].value == "74LS04");
    CHECK(parts[1].reference == "R1");
    CHECK(parts[1].value == "4k7");
    CHECK(parts[2].reference == "U1");
}

TEST_CASE("builder: connections include implicit power and resistor pins", "[netlist]") {
    const auto result =
        LoadNetlist("part U1 74LS04\npart R1 R\nnet VCC R1.1\nnet A R1.2 U1.1A U1.3\n", "t.bwn");
    REQUIRE(result.has_value());
    std::vector<std::string> connections;
    for (const Connection& c : (*result)->connections) {
        connections.push_back(c.reference + "." + std::to_string(c.pin + 1) + " " + c.net);
    }
    CHECK(connections == Lines{"R1.1 VCC", "R1.2 A", "U1.1 A", "U1.3 A", "U1.7 GND", "U1.14 VCC"});
}
