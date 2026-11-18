#include "view/board_marks.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("board marks: a capacitor's code", "[view]") {
    CHECK(CapacitorCode(47) == "47");
    CHECK(CapacitorCode(10) == "10");
    CHECK(CapacitorCode(5) == "5");
    CHECK(CapacitorCode(100) == "101");
    CHECK(CapacitorCode(330) == "331");
    CHECK(CapacitorCode(4'700) == "472");
    CHECK(CapacitorCode(47'000) == "473");
    CHECK(CapacitorCode(100'000) == "104");
    CHECK(CapacitorCode(1'000'000) == "105");
    CHECK(CapacitorCode(2'200'000) == "225");
    CHECK(CapacitorCode(999) == "102");  // rounded to two digits: 1000 pF
}
