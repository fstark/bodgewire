#include "netlist/diagnostic.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("diagnostic: format error and warning", "[netlist]") {
    CHECK(Format({"board.bwn", 12, Severity::Error, "U3 (74LS00) has no pin 15"}) ==
          "board.bwn:12: error: U3 (74LS00) has no pin 15");
    CHECK(Format({"board.bwn", 3, Severity::Warning, "U2.1 (1A) is unconnected: floats high"}) ==
          "board.bwn:3: warning: U2.1 (1A) is unconnected: floats high");
}

TEST_CASE("diagnostic: line 0", "[netlist]") {
    CHECK(Format({"board.bwn", 0, Severity::Error, "cannot read file"}) ==
          "board.bwn: error: cannot read file");
}
