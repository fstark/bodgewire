#include "view/meter_reading.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("meter reading: volts with two decimals", "[view]") {
    CHECK(VoltsReading(2.434) == MeterReading{"2.43"});
    CHECK(VoltsReading(0.0) == MeterReading{"0.00"});
    CHECK(VoltsReading(4.996) == MeterReading{"5.00"});
}

TEST_CASE("meter reading: ten volts and over with one", "[view]") {
    CHECK(VoltsReading(12.0) == MeterReading{"12.0"});
    CHECK(VoltsReading(9.996) == MeterReading{"10.0"});
}

TEST_CASE("meter reading: no reading is dashes", "[view]") {
    CHECK(VoltsReading(std::nullopt) == MeterReading{"---"});
}

TEST_CASE("meter reading: continuity", "[view]") {
    CHECK(ContinuityReading(true) == MeterReading{"0.0", true, true});
    CHECK(ContinuityReading(false) == MeterReading{"OL", true, false});
    CHECK(ContinuityReading(std::nullopt) == MeterReading{"OL", true, false});
}

TEST_CASE("meter reading: segments", "[view]") {
    CHECK(Segments('8') == 127);
    CHECK(Segments('-') == 64);
    CHECK(Segments('L') == (8 | 16 | 32));
    CHECK(Segments('O') == Segments('0'));
    CHECK(Segments(' ') == 0);
}
