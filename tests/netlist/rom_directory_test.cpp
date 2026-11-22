#include "netlist/rom_directory.hpp"

#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "netlist/builder.hpp"
#include "parts/memory_cells.hpp"

using namespace bw;

namespace {

// A fresh temporary directory for one case.
std::filesystem::path TempDir(const std::string& name) {
    const auto dir = std::filesystem::temp_directory_path() / ("bw_rom_directory_" + name);
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void Write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream(path, std::ios::binary) << text;
}

// Two 2316B sockets as the BW-2001's, one filled from a two-chip file with offset=.
Netlist TwoSockets(const std::filesystem::path& dir) {
    Write(dir / "ours.rom", "@0000 11\n@0800 22\n");
    const std::string text = "part J1 POWER\n"
                             "part UH1 2316B select=LLH contents=ours.rom offset=0\n"
                             "part UH5 2316B select=LHH contents=ours.rom offset=$800\n"
                             "part R1 R value=1k\n"
                             "net VCC R1.1\nnet PU R1.2\n";
    auto netlist = ParseNetlist(text, "t.bwn");
    REQUIRE(netlist.has_value());
    ResolveContentsPaths(*netlist, dir);
    return std::move(*netlist);
}

std::string Param(const Netlist& netlist, const std::string& reference, const std::string& name) {
    for (const PartDecl& part : netlist.parts) {
        if (part.reference == reference) {
            for (const PartParam& param : part.params) {
                if (param.name == name) {
                    return param.value;
                }
            }
        }
    }
    return "";
}

}  // namespace

TEST_CASE("rom directory: a file named by reference replaces the Part's contents", "[netlist]") {
    const auto base = TempDir("replace");
    Netlist netlist = TwoSockets(base);
    std::filesystem::create_directories(base / "set");
    Write(base / "set" / "UH5.bin", std::string(2048, '\x5A'));
    Write(base / "set" / "README.md", "not a ROM");
    REQUIRE(ApplyRomDirectory(netlist, base / "set").has_value());
    CHECK(Param(netlist, "UH5", "contents") == (base / "set" / "UH5.bin").lexically_normal());
    CHECK(Param(netlist, "UH5", "offset").empty());  // dropped: the file is one chip
    CHECK(Param(netlist, "UH1", "offset") == "0");   // no file: as the netlist says
    CHECK(Param(netlist, "UH1", "contents") == (base / "ours.rom").lexically_normal());
    auto built = BuildCircuit(netlist);
    if (!built) {
        for (const Diagnostic& d : built.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(built.has_value());
    const auto* cells = dynamic_cast<const MemoryCells*>((*built)->components.at("UH5"));
    REQUIRE(cells != nullptr);
}

TEST_CASE("rom directory: a .rom text file works as well", "[netlist]") {
    const auto base = TempDir("text");
    Netlist netlist = TwoSockets(base);
    std::filesystem::create_directories(base / "set");
    Write(base / "set" / "UH1.rom", "@0000 A9\n");
    REQUIRE(ApplyRomDirectory(netlist, base / "set").has_value());
    CHECK(Param(netlist, "UH1", "contents").ends_with("UH1.rom"));
    CHECK(BuildCircuit(netlist).has_value());
}

TEST_CASE("rom directory: refusals", "[netlist]") {
    const auto base = TempDir("refusals");
    SECTION("no directory") {
        Netlist netlist = TwoSockets(base);
        const auto applied = ApplyRomDirectory(netlist, base / "missing");
        REQUIRE_FALSE(applied.has_value());
        CHECK(applied.error() == "no ROM directory " + (base / "missing").string());
    }
    SECTION("a file for no ROM") {
        Netlist netlist = TwoSockets(base);
        std::filesystem::create_directories(base / "set");
        Write(base / "set" / "UX9.bin", "x");
        const auto applied = ApplyRomDirectory(netlist, base / "set");
        REQUIRE_FALSE(applied.has_value());
        CHECK(applied.error().ends_with("UX9.bin matches no ROM in t.bwn"));
    }
    SECTION("a file for a Part that is not a ROM") {
        Netlist netlist = TwoSockets(base);
        std::filesystem::create_directories(base / "set");
        Write(base / "set" / "R1.bin", "x");
        CHECK_FALSE(ApplyRomDirectory(netlist, base / "set").has_value());
    }
    SECTION("two files for one ROM") {
        Netlist netlist = TwoSockets(base);
        std::filesystem::create_directories(base / "set");
        Write(base / "set" / "UH1.bin", std::string(2048, '\0'));
        Write(base / "set" / "UH1.rom", "@0000 00\n");
        const auto applied = ApplyRomDirectory(netlist, base / "set");
        REQUIRE_FALSE(applied.has_value());
        CHECK(applied.error().find("two files for UH1") != std::string::npos);
    }
}
