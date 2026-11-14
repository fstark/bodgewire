#include "view/time_text.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

TEST_CASE("time text: in three significant digits", "[view]") {
    CHECK(FormatTime(0, true) == "0");
    CHECK(FormatTime(1'700'000, false) == "1.7 µs");
    CHECK(FormatTime(6_us, true) == "+6 µs");
    CHECK(FormatTime(-3_us, true) == "-3 µs");
    CHECK(FormatTime(655'300'000, false) == "655 µs");
    CHECK(FormatTime(6'553'600'000, false) == "6.55 ms");
}

TEST_CASE("time text: frequency of a period", "[view]") {
    CHECK(FormatFrequency(1'700'000) == "588 kHz");
    CHECK(FormatFrequency(600_ns) == "1.67 MHz");
    CHECK(FormatFrequency(100_ns) == "10 MHz");
    CHECK(FormatFrequency(20_ms) == "50 Hz");
}
