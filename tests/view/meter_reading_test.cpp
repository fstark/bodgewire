#include "view/meter_reading.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("meter reading: volts with two decimals", "[view]") {
    CHECK(VoltsReading(2.434) == MeterReading{"2.43", true});
    CHECK(VoltsReading(0.0) == MeterReading{"0.00", true});
    CHECK(VoltsReading(4.996) == MeterReading{"5.00", true});
}

TEST_CASE("meter reading: ten volts and over with one", "[view]") {
    CHECK(VoltsReading(12.0) == MeterReading{"12.0", true});
    CHECK(VoltsReading(9.996) == MeterReading{"10.0", true});
}

TEST_CASE("meter reading: no reading is dashes", "[view]") {
    CHECK(VoltsReading(std::nullopt) == MeterReading{"----", true});
}

TEST_CASE("meter reading: continuity", "[view]") {
    CHECK(ContinuityReading(true) == MeterReading{"bEEP", false, true});
    CHECK(ContinuityReading(false) == MeterReading{"----", false, false});
    CHECK(ContinuityReading(std::nullopt) == MeterReading{"----", false, false});
}

TEST_CASE("meter reading: segments", "[view]") {
    CHECK(Segments('8') == 127);
    CHECK(Segments('-') == 64);
    CHECK(Segments('b') == (4 | 8 | 16 | 32 | 64));
    CHECK(Segments('E') == (1 | 8 | 16 | 32 | 64));
    CHECK(Segments('P') == (1 | 2 | 16 | 32 | 64));
    CHECK(Segments(' ') == 0);
}
