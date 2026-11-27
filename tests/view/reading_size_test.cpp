#include "view/reading_size.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("reading size: the aA key steps 16, 20, 24, then back to 16", "[view]") {
    CHECK(NextReadingSize(16) == 20);
    CHECK(NextReadingSize(20) == 24);
    CHECK(NextReadingSize(24) == 16);
    CHECK(NextReadingSize(18) == 16);  // not a step: the first
}

TEST_CASE("reading size: the settings file's text-size line, written and read back", "[view]") {
    CHECK(SettingsText(20) == "# Bodgewire settings\ntext-size 20\n");
    CHECK(ParseReadingSize(SettingsText(24)) == 24);
    CHECK(ParseReadingSize("  text-size   20  \r\n") == 20);
    // No line, a size not offered, or not a number: 16.
    CHECK(ParseReadingSize("") == 16);
    CHECK(ParseReadingSize("# Bodgewire settings\n") == 16);
    CHECK(ParseReadingSize("text-size 18\n") == 16);
    CHECK(ParseReadingSize("text-size 20px\n") == 16);
    CHECK(ParseReadingSize("text-size\n") == 16);
}
