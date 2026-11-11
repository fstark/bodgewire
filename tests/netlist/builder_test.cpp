#include "netlist/builder.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/ram2114.hpp"
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

TEST_CASE("builder: a ROM without contents", "[netlist]") {
    CHECK(Errors("part U1 2716\n") == Lines{"1: U1: 2716 needs contents=<file>"});
}

TEST_CASE("builder: contents= is found beside the netlist file", "[netlist]") {
    const auto dir = std::filesystem::temp_directory_path() / "bw_builder_contents";
    std::filesystem::create_directories(dir / "netlists");
    std::filesystem::create_directories(dir / "roms");
    std::ofstream(dir / "roms" / "x.rom") << "\"A\"\n";
    std::ofstream(dir / "netlists" / "x.bwn")
        << "part U1 2716 contents=../roms/x.rom\nnet GND U1.A0 U1.A1 U1.A2 U1.A3 U1.A4 U1.A5\n"
           "net GND U1.A6 U1.A7 U1.A8 U1.A9 U1.A10 U1.CE_N U1.OE_N\nnet VCC U1.VPP\n";
    const auto loaded = LoadNetlistFile(dir / "netlists" / "x.bwn");
    REQUIRE(loaded.has_value());
    CHECK((*loaded)->warnings.empty());

    std::ofstream(dir / "roms" / "x.rom") << "zz\n";
    const auto bad = LoadNetlistFile(dir / "netlists" / "x.bwn");
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error()[0].line == 1);
    CHECK(bad.error()[0].message == "U1: 2716: x.rom line 1: unknown token 'zz'");
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

TEST_CASE("builder: an unconnected digit pin gives no warning", "[netlist]") {
    // DP left unconnected: a MOS-family load is dark, not "floating high".
    const auto result = LoadNetlist("part J1 POWER\npart R1 R value=330\npart DS1 DIGIT\n"
                                    "net VCC R1.1\nnet CA R1.2 DS1.CA1 DS1.CA2\n"
                                    "net GND DS1.A\n",
                                    "t.bwn");
    REQUIRE(result.has_value());
    CHECK((*result)->warnings.empty());
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

TEST_CASE("rails: declared voltages", "[netlist]") {
    const auto result = LoadNetlistFile(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    REQUIRE(result.has_value());
    const LoadedCircuit& loaded = **result;
    CHECK(RailVolts(loaded, loaded.nets.at("VCC")) == 5.0);
    CHECK(RailVolts(loaded, loaded.nets.at("GND")) == 0.0);
    CHECK_FALSE(RailVolts(loaded, loaded.nets.at("CLK")).has_value());
    CHECK_FALSE(RailVolts(loaded, loaded.nets.at("VIDEO")).has_value());
}

TEST_CASE("builder: stuck pin ties its net", "[netlist]") {
    // 1Y drives 2A on net A; 1A is on GND, so a working 1Y would drive 1.
    const auto netlist = ParseNetlist("part U1 74LS04\nnet A U1.2 U1.3\nnet GND U1.1\n", "t.bwn");
    REQUIRE(netlist.has_value());
    const StuckPin stuck[] = {{"U1", 1, Zero}};
    const auto result = BuildCircuit(*netlist, stuck);
    REQUIRE(result.has_value());
    LoadedCircuit& loaded = **result;
    const NetId a = loaded.nets.at("A");
    CHECK(loaded.circuit.tieOf(a).strength == DriveStrength::Strong);
    CHECK(loaded.circuit.tieOf(a).value == Zero);
    const PinId pin2{ToIndex(loaded.components.at("U1")->firstPin()) + 1};
    CHECK(loaded.circuit.netOf(pin2) != a);
    CHECK(PinNet(loaded, "U1", "1Y") == a);
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    CHECK(sim.value(a) == Zero);
    CHECK(sim.value(loaded.circuit.netOf(pin2)) == One);
}

TEST_CASE("builder: stuck input reads its value, its Net unchanged", "[netlist]") {
    // 74LS00: 1A and 2A on net A, held 1 by a pull-up; 1B and 2B on VCC. 1A stuck at 0.
    const auto netlist = ParseNetlist("part U1 74LS00\npart R1 R value=4k7\n"
                                      "net A R1.2 U1.1 U1.4\nnet VCC R1.1 U1.2 U1.5\n"
                                      "net Y1 U1.3\nnet Y2 U1.6\n",
                                      "t.bwn");
    REQUIRE(netlist.has_value());
    const StuckPin stuck[] = {{"U1", 0, Zero}};  // 1A
    const auto result = BuildCircuit(*netlist, stuck);
    REQUIRE(result.has_value());
    LoadedCircuit& loaded = **result;
    const NetId a = loaded.nets.at("A");
    CHECK(PinNet(loaded, "U1", "1A") == a);
    CHECK(loaded.circuit.tieOf(a).strength == DriveStrength::Weak);
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    sim.runUntil(100_ns);
    CHECK(sim.value(a) == One);
    CHECK(sim.value(loaded.nets.at("Y1")) == One);   // NAND(0, 1): the stuck input
    CHECK(sim.value(loaded.nets.at("Y2")) == Zero);  // NAND(1, 1): a good input on A
}

namespace {

// A 2114 always writing address 0 (CS_N, WE_N and the address on GND), IO1 on net B0, which a
// 74LS04 output may also drive; the other IO pins unconnected.
constexpr std::string_view kRamWriting =
    "part U1 2114\n"
    "net GND U1.CS_N U1.WE_N U1.A0 U1.A1 U1.A2 U1.A3 U1.A4 U1.A5 U1.A6 U1.A7 U1.A8 U1.A9\n";

}  // namespace

TEST_CASE("builder: stuck bidirectional pin forces its net and is read by its chip", "[netlist]") {
    const StuckPin stuck[] = {{"U1", Ram2114::kData[0], One}};
    for (const bool fight : {false, true}) {
        INFO(fight);
        // With the fight, a 74LS04 drives 0 on B0 (its 1A on VCC).
        const std::string text = std::string(kRamWriting) +
                                 (fight ? "part U2 74LS04\nnet B0 U1.IO1 U2.1Y\nnet VCC U2.1A\n"
                                          "net GND U2.3 U2.5 U2.9 U2.11 U2.13\n"
                                        : "net B0 U1.IO1\n");
        const auto netlist = ParseNetlist(text, "t.bwn");
        REQUIRE(netlist.has_value());
        const auto result = BuildCircuit(*netlist, stuck);
        REQUIRE(result.has_value());
        LoadedCircuit& loaded = **result;
        CHECK(loaded.warnings.empty());
        const NetId b0 = loaded.nets.at("B0");
        CHECK(loaded.circuit.tieOf(b0).value == One);
        Simulator sim(loaded.circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        // A Strong TTL tie against a TTL 0: 0 wins, as for a stuck output.
        CHECK(sim.value(b0) == (fight ? Zero : One));
        const auto& ram = static_cast<const Ram2114&>(*loaded.components.at("U1"));
        CHECK((ram.word(0) & 1) == 1);  // the chip reads its stuck value, fight or not
    }
}

TEST_CASE("builder: an unconnected MOS bidirectional pin gives no warning", "[netlist]") {
    const auto result = LoadNetlist(std::string(kRamWriting), "t.bwn");
    REQUIRE(result.has_value());
    CHECK((*result)->warnings.empty());
}

TEST_CASE("builder: a bidirectional pin off its net drives nothing there", "[netlist]") {
    // A dry Joint's electrical netlist: U1.IO1 left off B0, which a 74LS04 drives to 1. U1 reads
    // (WE_N on VCC), so with IO1 on B0 it would fight the inverter on some seeds.
    for (std::uint64_t seed = 1; seed <= 8; ++seed) {
        INFO(seed);
        const auto result = LoadNetlist(
            "part U1 2114\npart U2 74LS04\nnet B0 U2.1Y\nnet VCC U1.WE_N\n"
            "net GND U2.1A U2.3 U2.5 U2.9 U2.11 U2.13 U1.CS_N U1.A0 U1.A1 U1.A2 U1.A3 U1.A4 "
            "U1.A5 U1.A6 U1.A7 U1.A8 U1.A9\n",
            "t.bwn");
        REQUIRE(result.has_value());
        LoadedCircuit& loaded = **result;
        const NetId b0 = loaded.nets.at("B0");
        CHECK(PinNet(loaded, "U1", "IO1") != b0);
        Simulator sim(loaded.circuit);
        sim.powerOn(seed);
        sim.runUntil(1_us);
        CHECK(sim.value(b0) == One);
        CHECK_FALSE(sim.isFloating(*PinNet(loaded, "U1", "IO1")));  // its own drive
    }
}

TEST_CASE("builder: unpowered chip drives nothing", "[netlist]") {
    std::ifstream in(std::filesystem::path(BW_NETLIST_DIR) / "test_pattern.bwn");
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const auto netlist = ParseNetlist(text, "test_pattern.bwn");
    REQUIRE(netlist.has_value());
    const std::string unpowered[] = {"U1"};
    const StuckPin stuck[] = {{"U1", 13, One}};  // QA
    const auto result = BuildCircuit(*netlist, stuck, unpowered);
    REQUIRE(result.has_value());
    LoadedCircuit& loaded = **result;
    const NetId h0 = loaded.nets.at("H0");
    CHECK(PinNet(loaded, "U1", "QA") == h0);
    CHECK(loaded.circuit.tieOf(h0).strength == DriveStrength::None);
    Simulator sim(loaded.circuit);
    sim.powerOn(1);
    sim.runUntil(10_us);
    CHECK(sim.isFloating(h0));
    CHECK_FALSE(sim.isFloating(loaded.nets.at("H4")));
}

TEST_CASE("builder: PinNet by name and number", "[netlist]") {
    const auto result = LoadNetlist("part U1 74LS04\nnet A U1.2 U1.3\n", "t.bwn");
    REQUIRE(result.has_value());
    const LoadedCircuit& loaded = **result;
    CHECK(PinNet(loaded, "U1", "1Y") == loaded.nets.at("A"));
    CHECK(PinNet(loaded, "U1", "3") == loaded.nets.at("A"));
    CHECK(PinNet(loaded, "U1", "14") == loaded.nets.at("VCC"));
    const auto unconnected = PinNet(loaded, "U1", "4");
    REQUIRE(unconnected.has_value());
    CHECK(*unconnected != loaded.nets.at("A"));
}

TEST_CASE("builder: PinNet unknown", "[netlist]") {
    const auto result = LoadNetlist("part U1 74LS04\nnet A U1.2 U1.3\n", "t.bwn");
    REQUIRE(result.has_value());
    CHECK_FALSE(PinNet(**result, "U2", "1Y").has_value());
    CHECK_FALSE(PinNet(**result, "U1", "9Y").has_value());
    CHECK_FALSE(PinNet(**result, "U1", "15").has_value());
    CHECK_FALSE(PinNet(**result, "U1", "0").has_value());
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

namespace {

constexpr std::string_view kButtonNetlist = "part R1 R value=4k7\n"
                                            "part SW1 BUTTON\n"
                                            "part U1 74LS04\n"
                                            "net VCC R1.1\n"
                                            "net BTN_N R1.2 SW1.1 U1.1A\n"
                                            "net GND SW1.2\n"
                                            "net OUT U1.1Y\n";

}  // namespace

TEST_CASE("builder: a button pulls its signal to the rail while pressed", "[netlist]") {
    for (const std::string_view text :
         {kButtonNetlist, std::string_view("part R1 R value=4k7\npart SW1 BUTTON\n"
                                           "part U1 74LS04\nnet VCC R1.1\n"
                                           "net BTN_N R1.2 SW1.2 U1.1A\nnet GND SW1.1\n"
                                           "net OUT U1.1Y\n")}) {
        auto result = LoadNetlist(text, "t.bwn");
        REQUIRE(result.has_value());
        const LoadedCircuit& loaded = **result;
        CHECK(PinNet(loaded, "SW1", "1") != PinNet(loaded, "SW1", "2"));
        const NetId gnd = loaded.nets.at("GND");
        const NetId signal = loaded.nets.at("BTN_N");
        CHECK((PinNet(loaded, "SW1", "1") == gnd || PinNet(loaded, "SW1", "2") == gnd));
        Button* button = ButtonOf(loaded, "SW1");
        REQUIRE(button != nullptr);
        Simulator sim((*result)->circuit);
        sim.powerOn(1);
        sim.runUntil(1_us);
        CHECK(sim.value(signal) == One);
        CHECK(sim.value(loaded.nets.at("OUT")) == Zero);
        button->setPressed(true);
        sim.wake(button->id());
        sim.runUntil(2_us);
        CHECK(sim.value(signal) == Zero);
        CHECK(sim.value(loaded.nets.at("OUT")) == One);
        CHECK(sim.value(gnd) == Zero);
    }
}

TEST_CASE("builder: a button needs one rail end", "[netlist]") {
    const std::string message =
        "2: SW1: a button needs one pin on GND or VCC and the other on a signal net";
    CHECK(Errors("part U1 74LS04\npart SW1 BUTTON\nnet A SW1.1 U1.1A\nnet B SW1.2 U1.3A\n") ==
          Lines{message});
    CHECK(Errors("part U1 74LS04\npart SW1 BUTTON\nnet VCC SW1.1\nnet GND SW1.2\n") ==
          Lines{message});
    CHECK(Errors("part U1 74LS04\npart SW1 BUTTON\nnet GND SW1.1\n") == Lines{message});
}

TEST_CASE("builder: ButtonOf", "[netlist]") {
    auto result = LoadNetlist(kButtonNetlist, "t.bwn");
    REQUIRE(result.has_value());
    CHECK(ButtonOf(**result, "SW1") != nullptr);
    CHECK(ButtonOf(**result, "R1") == nullptr);
    CHECK(ButtonOf(**result, "U1") == nullptr);
    CHECK(ButtonOf(**result, "SW9") == nullptr);
}
