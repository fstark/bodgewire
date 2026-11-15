#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/z80_core.hpp"
#include "z80_cases.hpp"

using namespace bw;
using namespace bw::test;

namespace {

const std::filesystem::path kData = BW_Z80_DATA_DIR;
const std::filesystem::path kExternal = BW_Z80_EXTERNAL_DIR;

// Every case of every file in `folder`; one failure message per file, at most.
void RunFolder(const std::filesystem::path& folder) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().extension() == ".txt") {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    std::size_t cases = 0, failed = 0;
    for (const auto& path : files) {
        const auto read = ReadZ80Cases(path);
        INFO(path.string());
        REQUIRE(read.has_value());
        for (const Z80Case& c : *read) {
            ++cases;
            if (const auto difference = RunZ80Case(c)) {
                ++failed;
                FAIL_CHECK(std::format("{}: {}", path.stem().string(), *difference));
                break;
            }
        }
    }
    INFO(cases << " cases, " << failed << " files failing");
    CHECK(files.size() == 1604);
}

// A CP/M program on the core alone (ZEXDOC, ZEXALL): loaded at $0100; BDOS calls 2 and 9 at
// $0005 printed into the result, then the RET there runs; a fetch at $0000 ends it.
std::string RunCpm(const std::filesystem::path& program, std::uint64_t cap,
                   std::uint64_t& tStates) {
    std::ifstream in(program, std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    std::vector<std::uint8_t> ram(0x10000, 0);
    std::ranges::copy(bytes, ram.begin() + 0x100);
    ram[0x0005] = 0xC9;  // RET
    ram[0x0006] = 0x00;  // the top of the program area, read as the stack's start
    ram[0x0007] = 0xF0;
    Z80Core core;
    core.start({.pc = 0x0100, .sp = 0xF000});
    std::string output;
    tStates = 0;
    while (tStates < cap) {
        const Z80Bus& bus = core.bus();
        std::uint8_t data = 0xFF;
        switch (bus.kind) {
        case Z80CycleKind::Fetch:
            if (bus.address == 0x0000) {
                return output;
            }
            if (bus.address == 0x0005 && !bus.halted) {
                const Z80Registers r = core.registers();
                if (r.c == 2) {
                    output += static_cast<char>(r.e);
                } else if (r.c == 9) {
                    for (unsigned a = static_cast<unsigned>(r.d << 8 | r.e); ram[a & 0xFFFF] != '$';
                         ++a) {
                        output += static_cast<char>(ram[a & 0xFFFF]);
                    }
                }
            }
            data = ram[bus.address];
            break;
        case Z80CycleKind::Read:
            data = ram[bus.address];
            break;
        case Z80CycleKind::Write:
            ram[bus.address] = bus.data;
            break;
        default:
            break;
        }
        tStates += static_cast<std::uint64_t>(Z80CycleLength(bus.kind, bus.length));
        core.step(data);
    }
    return output + "\n(cap reached)";
}

void RunZex(const char* name) {
    const auto path = kExternal / "zex" / name;
    if (!std::filesystem::exists(path)) {
        SKIP("no external/z80/zex: run tests/data/z80/fetch.sh");
    }
    std::uint64_t tStates = 0;
    const std::string output = RunCpm(path, 100'000'000'000ull, tStates);
    INFO(output);
    INFO(tStates << " T-states");
    CHECK(output.find("ERROR") == std::string::npos);
    CHECK(output.find("Tests complete") != std::string::npos);
}

}  // namespace

TEST_CASE("Z80 core: Harte subset", "[parts]") { RunFolder(kData / "harte"); }

TEST_CASE("Z80 core: Harte full", "[.][slow]") {
    if (!std::filesystem::exists(kExternal / "harte")) {
        SKIP("no external/z80/harte: run tests/data/z80/fetch.sh");
    }
    RunFolder(kExternal / "harte");
}

TEST_CASE("Z80 core: ZEXDOC", "[.][slow]") { RunZex("zexdoc.com"); }

TEST_CASE("Z80 core: ZEXALL", "[.][slow]") { RunZex("zexall.com"); }
