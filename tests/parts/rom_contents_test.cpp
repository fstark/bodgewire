#include "parts/rom_contents.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../paths.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

std::vector<std::uint8_t> Words(std::string_view text, std::size_t size = 16, int width = 8) {
    auto words = ParseRomContents(text, "t.rom", size, width);
    INFO((words ? std::string() : words.error()));
    REQUIRE(words.has_value());
    return *words;
}

std::string Error(std::string_view text, std::size_t size = 16, int width = 8) {
    auto words = ParseRomContents(text, "t.rom", size, width);
    REQUIRE_FALSE(words.has_value());
    return words.error();
}

}  // namespace

TEST_CASE("rom contents: hex words, repeats and addresses", "[parts]") {
    const auto words = Words("01 2 ff ; three words\n@8 AB*3\n@00F 7");
    CHECK(words[0] == 0x01);
    CHECK(words[1] == 0x02);
    CHECK(words[2] == 0xFF);
    CHECK(words[3] == 0xFF);  // unwritten
    CHECK(words[8] == 0xAB);
    CHECK(words[9] == 0xAB);
    CHECK(words[10] == 0xAB);
    CHECK(words[11] == 0xFF);
    CHECK(words[15] == 0x07);
}

TEST_CASE("rom contents: strings with escapes", "[parts]") {
    const auto words = Words(R"("A;\"\\" 20)");
    CHECK(words[0] == 'A');
    CHECK(words[1] == ';');  // a comment only outside a string
    CHECK(words[2] == '"');
    CHECK(words[3] == '\\');
    CHECK(words[4] == 0x20);
}

TEST_CASE("rom contents: dot rows", "[parts]") {
    const auto words = Words(".##.#\n#####  .....\n#", 8, 5);
    CHECK(words[0] == 0x0D);
    CHECK(words[1] == 0x1F);
    CHECK(words[2] == 0x00);
    CHECK(words[3] == 0x01);
    CHECK(words[4] == 0x1F);  // unwritten: all ones in 5 bits
}

TEST_CASE("rom contents: empty file is all ones", "[parts]") {
    const auto words = Words("; nothing\n\n", 4, 5);
    CHECK(words == std::vector<std::uint8_t>(4, 0x1F));
}

TEST_CASE("rom contents: errors name the file and line", "[parts]") {
    CHECK(Error("00\nzz") == "t.rom line 2: unknown token 'zz'");
    CHECK(Error("123") == "t.rom line 1: unknown token '123'");
    CHECK(Error("01*0") == "t.rom line 1: unknown token '01*0'");
    CHECK(Error("@xyz") == "t.rom line 1: bad address '@xyz'");
    CHECK(Error("@") == "t.rom line 1: bad address '@'");
    CHECK(Error("20", 16, 5) == "t.rom line 1: word 0x20 is wider than 5 bits");
    CHECK(Error("\"A\"", 16, 5) == "t.rom line 1: word 0x41 is wider than 5 bits");
    CHECK(Error("######", 16, 5) == "t.rom line 1: dot row '######' is wider than 5 bits");
    CHECK(Error(".#x") == "t.rom line 1: unknown token '.#x'");
    CHECK(Error("@F 1 2") == "t.rom line 1: address 0x010 is past the end (size 0x010)");
    CHECK(Error("1\n\n@0 2") == "t.rom line 3: address 0x000 already written at line 1");
    CHECK(Error("\"AB") == "t.rom line 1: unterminated string");
    CHECK(Error(R"("\n")") == "t.rom line 1: bad escape '\\n'");
}

TEST_CASE("rom contents: missing file", "[parts]") {
    const auto words = ReadRomContents("/nonexistent/x.rom", 4, 8);
    REQUIRE_FALSE(words.has_value());
    CHECK(words.error() == "cannot read '/nonexistent/x.rom'");
}

namespace {

std::filesystem::path WriteBytes(const std::string& name, std::size_t count) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path, std::ios::binary);
    for (std::size_t i = 0; i < count; ++i) {
        out.put(static_cast<char>((i * 7 + 3) & 0xFF));
    }
    return path;
}

}  // namespace

TEST_CASE("rom contents: a .bin is a raw dump", "[parts]") {
    const auto path = WriteBytes("bw_rom_contents_test.bin", 2048);
    const auto words = ReadRomContents(path, 2048, 8);
    REQUIRE(words.has_value());
    REQUIRE(words->size() == 2048);
    for (std::size_t i = 0; i < words->size(); ++i) {
        REQUIRE((*words)[i] == ((i * 7 + 3) & 0xFF));
    }
    const auto upper = WriteBytes("bw_rom_contents_test.BIN", 16);
    CHECK(ReadRomContents(upper, 16, 8).has_value());
}

TEST_CASE("rom contents: a .bin of the wrong size or width", "[parts]") {
    const auto path = WriteBytes("bw_rom_contents_short.bin", 2047);
    const auto words = ReadRomContents(path, 2048, 8);
    REQUIRE_FALSE(words.has_value());
    CHECK(words.error() == "bw_rom_contents_short.bin: 2047 bytes, expected 2048");
    const auto narrow = ReadRomContents(path, 2047, 5);
    REQUIRE_FALSE(narrow.has_value());
    CHECK(narrow.error() == "bw_rom_contents_short.bin: a .bin holds 8-bit words");
}

TEST_CASE("rom contents: the Machine files parse", "[parts]") {
    CHECK(ReadRomContents(test::RomPath("message", "2513.rom"), 512, 5).has_value());
    // The PET's character ROM, shipped with the BW-2001 (decided 2026-11-15): a 2316B's dump.
    const auto characters = ReadRomContents(test::RomPath("bw2001", "901447-08.bin"), 2048, 8);
    REQUIRE(characters.has_value());
    // Screen code 1, 'A': its first row of dots is $18 (MAME's pet2001 character ROM).
    CHECK((*characters)[8] == 0x18);
}
