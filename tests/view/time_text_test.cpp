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

TEST_CASE("time text: with the digits a step needs", "[view]") {
    CHECK(FormatTimeStep(98'450'000, 50_ns, false) == "98.45 µs");  // the scope's position
    CHECK(FormatTimeStep(98'400'000, 50_ns, false) == "98.4 µs");
    CHECK(FormatTimeStep(100'500'000, 500_ns, true) == "+100.5 µs");
    CHECK(FormatTimeStep(-1'250'000, 50_ns, true) == "-1.25 µs");
    CHECK(FormatTimeStep(1_ms, 200_us, false) == "1 ms");
    CHECK(FormatTimeStep(500_ns, 500_ns, false) == "500 ns");
    CHECK(FormatTimeStep(0, 1_us, true) == "0");
}

TEST_CASE("time text: frequency of a period", "[view]") {
    CHECK(FormatFrequency(1'700'000) == "588 kHz");
    CHECK(FormatFrequency(600_ns) == "1.67 MHz");
    CHECK(FormatFrequency(100_ns) == "10 MHz");
    CHECK(FormatFrequency(20_ms) == "50 Hz");
}
