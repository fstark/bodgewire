#include "view/contents_defaults.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "netlist/parser.hpp"
#include "parts/rom.hpp"

using namespace bw;

namespace {

std::string Error(std::string_view text) {
    const auto parsed = ParseContentsDefaults(text, "contents.txt");
    REQUIRE_FALSE(parsed.has_value());
    return parsed.error();
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST_CASE("contents defaults: each line", "[contents]") {
    const auto none = ParseContentsDefaults("# nothing\n\n", "c");
    REQUIRE(none.has_value());
    CHECK_FALSE(none->cpu.has_value());
    CHECK(none->charset == CharacterSet::Ascii);
    CHECK(none->chips.empty());

    const auto parsed = ParseContentsDefaults("cpu z80\ncharset zx80  # the ZX80's\n"
                                              "chip IC2 address $0000 glyphs $0E00\n"
                                              "chip U9 glyphs 512\r\nchip U10\n",
                                              "c");
    REQUIRE(parsed.has_value());
    CHECK(parsed->cpu == Cpu::Z80);
    CHECK(parsed->charset == CharacterSet::Zx80);
    REQUIRE(parsed->chips.size() == 3);
    CHECK(parsed->chips.at("IC2").address == 0);
    CHECK(parsed->chips.at("IC2").glyphs == std::size_t{0xE00});
    CHECK_FALSE(parsed->chips.at("U9").address.has_value());
    CHECK(parsed->chips.at("U9").glyphs == std::size_t{512});
    CHECK_FALSE(parsed->chips.at("U10").glyphs.has_value());
    CHECK(ParseContentsDefaults("cpu 6502\ncharset petscii", "c")->cpu == Cpu::Mos6502);
}

TEST_CASE("contents defaults: errors name the line", "[contents]") {
    CHECK(Error("cpu 8080") == "contents.txt line 1: cpu takes 6502 or z80");
    CHECK(Error("cpu z80\ncpu z80") == "contents.txt line 2: a second cpu");
    CHECK(Error("charset ebcdic") == "contents.txt line 1: charset takes ascii, petscii or zx80");
    CHECK(Error("charset ascii\n# x\ncharset zx80") == "contents.txt line 3: a second charset");
    CHECK(Error("chip") == "contents.txt line 1: chip needs a reference");
    CHECK(Error("chip U1\nchip U1") == "contents.txt line 2: chip U1 twice");
    CHECK(Error("chip U1 address") == "contents.txt line 1: chip REF [address N] [glyphs N]");
    CHECK(Error("chip U1 address $G0") == "contents.txt line 1: bad number '$G0'");
    CHECK(Error("chip U1 address $10000") == "contents.txt line 1: address over $FFFF");
    CHECK(Error("chip U1 glyphs 1 glyphs 2") ==
          "contents.txt line 1: unknown or repeated 'glyphs'");
    CHECK(Error("chip U1 at $10") == "contents.txt line 1: unknown or repeated 'at'");
    CHECK(Error("rom U1") == "contents.txt line 1: unknown keyword 'rom'");
}

TEST_CASE("contents defaults: every Machine's file names its ROM chips", "[contents]") {
    int files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(test::RootDir() / "machines")) {
        if (!entry.is_directory()) {
            continue;
        }
        std::filesystem::path netlistPath;
        for (const auto& file : std::filesystem::directory_iterator(entry.path())) {
            if (file.path().extension() == ".bwn") {
                netlistPath = file.path();
            }
        }
        if (netlistPath.empty()) {
            continue;
        }
        INFO(netlistPath.filename().string());
        const auto netlist = ParseNetlist(ReadFile(netlistPath), netlistPath.filename().string());
        REQUIRE(netlist.has_value());
        // Each ROM chip with its word count; whether a CPU is on the Machine.
        std::map<std::string, std::size_t, std::less<>> roms;
        bool cpu = false;
        for (const PartDecl& part : netlist->parts) {
            if (const RomSpec* spec = FindRomSpec(part.part)) {
                roms[part.reference] = std::size_t{1} << spec->address.size();
            } else if (part.part == "6530") {
                roms[part.reference] = 1024;
            }
            cpu = cpu || part.part == "6502" || part.part == "Z80";
        }
        const auto path = entry.path() / "roms" / "contents.txt";
        if (!std::filesystem::exists(path)) {
            CHECK(roms.empty());  // a Machine with ROMs has its defaults
            continue;
        }
        ++files;
        const auto defaults = ParseContentsDefaults(ReadFile(path), "contents.txt");
        REQUIRE(defaults.has_value());
        CHECK(defaults->cpu.has_value() == (cpu && !roms.empty()));
        for (const auto& [reference, chip] : defaults->chips) {
            INFO(reference);
            const auto rom = roms.find(reference);
            REQUIRE(rom != roms.end());
            if (chip.glyphs) {
                CHECK(*chip.glyphs < rom->second);
            }
            if (chip.address) {
                CHECK(std::size_t{*chip.address} + rom->second <= 0x10000);
            }
        }
    }
    CHECK(files == 6);
}
