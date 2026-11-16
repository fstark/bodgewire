// Machine 120 BW-1 (machines/120-bw1/docs/BW1_DESIGN.md, Behaviour checked): the monitor and the
// self-test of P5k on the real Parts, keys pressed as a player would.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "bw1_rig.hpp"
#include "machine/machine.hpp"
#include "netlist/builder.hpp"
#include "netlist/parser.hpp"

using namespace bw;
using namespace bw::literals;

namespace {

std::unique_ptr<LoadedCircuit> DesignCircuit() {
    auto result = LoadNetlistFile(Bw1NetlistPath());
    REQUIRE(result.has_value());
    return std::move(*result);
}

// The Circuit built from the Board's Copper, with no Faults; with `strap`, a Bodge Wire from U24
// pad 4 (STRAP) to U2 pad 2 (PA0).
std::unique_ptr<LoadedCircuit> BoardCircuit(bool strap = false) {
    auto machine = LoadNetlistMachine(Bw1NetlistPath());
    REQUIRE(machine.has_value());
    if (strap) {
        REQUIRE(machine->addWire({"U24", "4"}, {"U2", "2"}));
    }
    auto built = machine->build();
    REQUIRE(built.has_value());
    return std::move(*built);
}

// The netlist with U24 output 3 joined to PA0: the self-test strap.
std::unique_ptr<LoadedCircuit> StrappedCircuit() {
    std::ifstream in(Bw1NetlistPath());
    REQUIRE(in);
    std::stringstream text;
    text << in.rdbuf();
    std::string source = text.str();
    const std::string strap = "net STRAP    U24.Y3_N";
    const std::size_t at = source.find(strap);
    REQUIRE(at != std::string::npos);
    source.replace(at, strap.size(), "net PA0      U24.Y3_N");
    auto netlist = ParseNetlist(source, "bw1.bwn");
    REQUIRE(netlist.has_value());
    ResolveContentsPaths(*netlist, Bw1NetlistPath().parent_path());
    auto result = BuildCircuit(*netlist);
    REQUIRE(result.has_value());
    CHECK((*result)->warnings.empty());
    return std::move(*result);
}

}  // namespace

TEST_CASE("bw1: netlist loads without warnings", "[machines]") {
    const auto loaded = DesignCircuit();
    CHECK(loaded->warnings.empty());
}

TEST_CASE("bw1: reset shows 0200", "[machines]") {
    const std::uint64_t seed = GENERATE(1, 2, 3);
    INFO("seed " << seed);
    Bw1 bw1 = Started(BoardCircuit(), seed);
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: keys set the address and write memory", "[machines][slow]") {
    Bw1 bw1 = Started(BoardCircuit(), 1);
    bw1.type("AD 0300 DA A9 +");
    CheckShows(bw1.display(), "0301");
    bw1.type("AD 0300");
    CHECK(bw1.display() == "0300A9");
}

TEST_CASE("bw1: a program typed in runs", "[machines][slow]") {
    Bw1 bw1 = Started(BoardCircuit(), 1);
    bw1.type("DA A9 + 42 + 85 + 10 + 00 AD 0200 GO");  // LDA #$42, STA $10, BRK
    CheckShows(bw1.display(), "0206");
    bw1.type("AD 0010");
    CHECK(bw1.display() == "001042");
}

TEST_CASE("bw1: ST stops a loop", "[machines][slow]") {
    Bw1 bw1 = Started(BoardCircuit(), 1);
    bw1.type("DA 4C + 00 + 02 AD 0200 GO");  // JMP $0200
    bw1.run(50_ms);
    bw1.press("ST");
    CHECK(bw1.display() == "02004C");
}

TEST_CASE("bw1: SST steps one instruction", "[machines][slow]") {
    Bw1 bw1 = Started(BoardCircuit(), 1);
    bw1.type("DA A9 + 11 + A9 + 22 AD 0200");  // LDA #$11, LDA #$22
    bw1.setSst(true);
    bw1.type("GO");
    CHECK(bw1.display() == "0202A9");
    bw1.type("GO");
    CheckShows(bw1.display(), "0204");  // past the program: RAM's power-on byte
}

TEST_CASE("bw1: RS returns to 0200", "[machines]") {
    Bw1 bw1 = Started(BoardCircuit(), 1);
    bw1.type("AD 1234");
    CheckShows(bw1.display(), "1234");
    bw1.press("RS");
    CheckShows(bw1.display(), "0200");
}

TEST_CASE("bw1: the strap runs the self-test", "[machines][slow]") {
    Bw1 bw1(StrappedCircuit(), 1);
    bw1.runUntil(22_ms);
    CHECK(bw1.display(5_ms) == "1     ");
    bw1.runUntil(300_ms);
    CHECK(bw1.display() == "888888");
    bw1.runUntil(3200_ms);
    CHECK(bw1.display() == "PA55  ");
    bw1.hold("GO");
    bw1.run(10_ms);
    CHECK(bw1.display() == "PA5513");
}

TEST_CASE("bw1: a Bodge Wire on the strap runs the self-test", "[machines][slow]") {
    Bw1 bw1(BoardCircuit(true), 1);
    bw1.runUntil(300_ms);
    CHECK(bw1.display() == "888888");
}
