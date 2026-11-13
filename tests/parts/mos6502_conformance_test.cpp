#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "mos6502_cases.hpp"
#include "parts/mos6502_core.hpp"

using namespace bw;
using namespace bw::test;

namespace {

const std::filesystem::path kData = BW_6502_DATA_DIR;
const std::filesystem::path kExternal = BW_6502_EXTERNAL_DIR;

// Every case of every opcode file in `folder`; one failure message per opcode, at most.
void RunFolder(const std::filesystem::path& folder) {
    std::size_t files = 0, cases = 0;
    for (int opcode = 0; opcode < 256; ++opcode) {
        const auto path = folder / std::format("{:02x}.txt", opcode);
        const auto read = ReadMos6502Cases(path);
        INFO(path.string());
        REQUIRE(read.has_value());
        ++files;
        for (const Mos6502Case& c : *read) {
            ++cases;
            if (const auto difference = RunMos6502Case(c)) {
                FAIL_CHECK(std::format("opcode {:02x}: {}", opcode, *difference));
                break;
            }
        }
    }
    CHECK(files == 256);
    INFO(cases << " cases");
}

}  // namespace

TEST_CASE("6502 core: Harte subset", "[parts]") { RunFolder(kData / "harte"); }

TEST_CASE("6502 core: Harte full", "[parts][slow]") {
    if (!std::filesystem::exists(kExternal / "harte")) {
        SKIP("no external/6502/harte: run tests/data/6502/fetch.sh");
    }
    RunFolder(kExternal / "harte");
}

TEST_CASE("6502 core: Dormann functional test", "[parts][slow]") {
    const auto folder = kExternal / "dormann";
    if (!std::filesystem::exists(folder / "6502_functional_test.bin")) {
        SKIP("no external/6502/dormann: run tests/data/6502/fetch.sh");
    }
    std::ifstream bin(folder / "6502_functional_test.bin", std::ios::binary);
    std::vector<std::uint8_t> ram((std::istreambuf_iterator<char>(bin)),
                                  std::istreambuf_iterator<char>());
    REQUIRE(ram.size() == 0x10000);
    std::ifstream successFile(folder / "success");
    std::string successText;
    successFile >> successText;
    const auto success = static_cast<std::uint16_t>(std::stoul(successText, nullptr, 16));

    Mos6502Core core;
    core.start({.pc = 0x0400, .a = 0, .x = 0, .y = 0, .s = 0xFF, .p = 0x24});
    std::uint16_t lastFetch = 0xFFFF;
    std::uint64_t cycles = 0;
    constexpr std::uint64_t kCap = 200'000'000;
    while (cycles < kCap) {
        const Mos6502Bus& bus = core.bus();
        if (bus.sync) {
            if (bus.address == lastFetch) {
                break;  // a trap: jmp * or a branch to itself
            }
            lastFetch = bus.address;
        }
        if (bus.read) {
            core.step(ram[bus.address]);
        } else {
            ram[bus.address] = bus.data;
            core.step(0);
        }
        ++cycles;
    }
    INFO(cycles << " cycles");
    CHECK(cycles < kCap);
    CHECK(std::format("{:04x}", lastFetch) == std::format("{:04x}", success));
}
