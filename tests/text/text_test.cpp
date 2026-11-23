#include "text/text.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

using Views = std::vector<std::string_view>;

TEST_CASE("text: ReadTextFile reads the bytes as they are", "[text]") {
    const auto path = std::filesystem::temp_directory_path() / "bw_text_test.txt";
    std::ofstream(path, std::ios::binary) << "a\r\nb\0c";
    CHECK(ReadTextFile(path) == std::string("a\r\nb"));
    std::filesystem::remove(path);
    CHECK_FALSE(ReadTextFile(path).has_value());
}

TEST_CASE("text: SplitWords splits at runs of white space", "[text]") {
    CHECK(SplitWords("part U1  7400\t# x\r") == Views{"part", "U1", "7400", "#", "x"});
    CHECK(SplitWords("  a\v\fb ") == Views{"a", "b"});
    CHECK(SplitWords("").empty());
    CHECK(SplitWords(" \t\r").empty());
}

TEST_CASE("text: Split keeps empty fields", "[text]") {
    CHECK(Split("a,,b", ',') == Views{"a", "", "b"});
    CHECK(Split("F.Cu:1.5:2", ':') == Views{"F.Cu", "1.5", "2"});
    CHECK(Split("a,", ',') == Views{"a", ""});
    CHECK(Split("", ',') == Views{""});
}

TEST_CASE("text: Trim drops white space at either end", "[text]") {
    CHECK(Trim("  a b \t\r\n") == "a b");
    CHECK(Trim("a") == "a");
    CHECK(Trim(" \t ").empty());
    CHECK(Trim("").empty());
}

TEST_CASE("text: ParseDecimal reads N or N.N at the start", "[text]") {
    const auto whole = ParseDecimal("14MHz");
    REQUIRE(whole.has_value());
    CHECK(whole->value == 14);
    CHECK(whole->length == 2);
    CHECK(whole->decimals == 0);
    const auto fraction = ParseDecimal("14.31818MHz");
    REQUIRE(fraction.has_value());
    CHECK(fraction->value == 14.31818);
    CHECK(fraction->length == 8);
    CHECK(fraction->decimals == 5);
    for (const char* bad : {"", "MHz", ".5", "-1", "5.", "5.x"}) {
        CHECK_FALSE(ParseDecimal(bad).has_value());
    }
}

TEST_CASE("text: ParsePercent reads N% from 0 to 100", "[text]") {
    CHECK(ParsePercent("0%") == 0.0);
    CHECK(ParsePercent("37.25%") == 37.25);
    CHECK(ParsePercent("100%") == 100.0);
    CHECK(ParsePercent("12.345%", 3) == 12.345);
    for (const char* bad : {"", "%", "50", "101%", "5.%", "-5%", "1e1%", "50%%", "50 %"}) {
        CHECK_FALSE(ParsePercent(bad).has_value());
    }
    CHECK_FALSE(ParsePercent("1.234%", 2).has_value());
}
