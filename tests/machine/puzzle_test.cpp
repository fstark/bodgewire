#include "machine/puzzle.hpp"

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

using Lines = std::vector<std::string>;

Lines Errors(std::string_view text) {
    const auto result = ParsePuzzle(text, "t.bwp");
    REQUIRE_FALSE(result.has_value());
    Lines errors;
    for (const Diagnostic& d : result.error()) {
        CHECK(d.file == "t.bwp");
        errors.push_back(std::to_string(d.line) + ": " + d.message);
    }
    return errors;
}

}  // namespace

TEST_CASE("puzzle: valid file", "[machine]") {
    const auto result = ParsePuzzle("# example\n"
                                    "\n"
                                    "machine test_pattern   # the board\n"
                                    "fault stuck U1.QC 0\n"
                                    "fault stuck U12.11 1\n"
                                    "fault crack F.Cu 52.30 41.00\n"
                                    "fault crack B.Cu -1 0.000001\n",
                                    "t.bwp");
    REQUIRE(result.has_value());
    const Puzzle& p = *result;
    CHECK(p.file == "t.bwp");
    CHECK(p.machine == "test_pattern");
    CHECK(p.machineLine == 3);
    REQUIRE(p.stuck.size() == 2);
    CHECK(p.stuck[0].line == 4);
    CHECK(p.stuck[0].reference == "U1");
    CHECK(p.stuck[0].pin == "QC");
    CHECK(p.stuck[0].value == LogicValue::Zero);
    CHECK(p.stuck[1].pin == "11");
    CHECK(p.stuck[1].value == LogicValue::One);
    REQUIRE(p.cracks.size() == 2);
    CHECK(p.cracks[0].line == 6);
    CHECK(p.cracks[0].layer == Layer::Front);
    CHECK(p.cracks[0].at == Point{52'300'000, 41'000'000});
    CHECK(p.cracks[1].layer == Layer::Back);
    CHECK(p.cracks[1].at == Point{-1'000'000, 1});
}

TEST_CASE("puzzle: errors", "[machine]") {
    CHECK(Errors("") == Lines{"0: no machine line"});
    CHECK(Errors("machine a\nmachine b\n") == Lines{"2: machine already given at line 1"});
    CHECK(Errors("machine\n") == Lines{"0: no machine line", "1: expected: machine NAME"});
    CHECK(Errors("machine ../x\n") == Lines{"0: no machine line", "1: bad machine name '../x'"});
    CHECK(Errors("machine m\nfoo\n") == Lines{"2: unknown directive 'foo'"});
    CHECK(Errors("machine m\nfault\nfault short U1.1\n") ==
          Lines{"2: expected: fault stuck|crack|bridge ...",
                "3: expected: fault stuck|crack|bridge ..."});
    CHECK(Errors("machine m\nfault stuck U1.QA\nfault stuck U1 0\nfault stuck U1.QA 2\n") ==
          Lines{"2: expected: fault stuck REF.PIN 0|1", "3: expected: fault stuck REF.PIN 0|1",
                "4: expected: fault stuck REF.PIN 0|1"});
    CHECK(
        Errors("machine m\nfault crack In1.Cu 1 2\nfault crack F.Cu 1\n") ==
        Lines{"2: expected: fault crack F.Cu|B.Cu X Y", "3: expected: fault crack F.Cu|B.Cu X Y"});
    CHECK(Errors("machine m\nfault crack F.Cu 1 2mm\n") == Lines{"2: bad coordinate '2mm'"});
    CHECK(Errors("machine m\nfault frob\n") == Lines{"2: expected: fault stuck|crack|bridge ..."});
    CHECK(Errors("machine m\nfault bridge U1.7\nfault bridge U17 U1.8\nfault bridge U1. U1.8\n") ==
          Lines{"2: expected: fault bridge REF.PIN REF.PIN",
                "3: expected: fault bridge REF.PIN REF.PIN",
                "4: expected: fault bridge REF.PIN REF.PIN"});
}

TEST_CASE("puzzle: bridge", "[machine]") {
    const auto result = ParsePuzzle("machine m\nfault bridge U1.7 U1.ENP  # same pad\n", "t.bwp");
    REQUIRE(result.has_value());
    REQUIRE(result->bridges.size() == 1);
    const BridgeDecl& b = result->bridges[0];
    CHECK(b.line == 2);
    CHECK(b.a.reference == "U1");
    CHECK(b.a.pin == "7");
    CHECK(b.b.reference == "U1");
    CHECK(b.b.pin == "ENP");
}

TEST_CASE("puzzle: title and brief", "[machine]") {
    const auto result = ParsePuzzle("machine m\n"
                                    "title \"Dead # counter\"   # comment\n"
                                    "  brief \"\"\n",
                                    "t.bwp");
    REQUIRE(result.has_value());
    CHECK(result->title == "Dead # counter");
    CHECK(result->titleLine == 2);
    CHECK(result->brief.empty());
    CHECK(result->briefLine == 3);

    const auto absent = ParsePuzzle("machine m\n", "t.bwp");
    REQUIRE(absent.has_value());
    CHECK(absent->title.empty());
    CHECK(absent->brief.empty());
    CHECK(absent->titleLine == 0);
}

TEST_CASE("puzzle: bad title or brief", "[machine]") {
    CHECK(Errors("machine m\ntitle \"open\n") == Lines{"2: expected: title \"TEXT\""});
    CHECK(Errors("machine m\ntitle bare\n") == Lines{"2: expected: title \"TEXT\""});
    CHECK(Errors("machine m\nbrief \"a\" b\n") == Lines{"2: expected: brief \"TEXT\""});
    CHECK(Errors("machine m\nbrief\n") == Lines{"2: expected: brief \"TEXT\""});
    CHECK(Errors("machine m\ntitle \"a\"\ntitle \"b\"\n") ==
          Lines{"3: title already given at line 2"});
}
