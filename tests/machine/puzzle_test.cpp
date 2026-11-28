#include "machine/puzzle.hpp"

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "machine/machine.hpp"
#include "machine/machine_list.hpp"

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

// The Puzzle's Fault `index` in file order, which is a `Decl`.
template <class Decl>
const Decl& FaultAt(const Puzzle& puzzle, std::size_t index) {
    REQUIRE(index < puzzle.faults.size());
    REQUIRE(std::holds_alternative<Decl>(puzzle.faults[index]));
    return std::get<Decl>(puzzle.faults[index]);
}

}  // namespace

TEST_CASE("puzzle: valid file", "[machine]") {
    const auto result = ParsePuzzle("# example\n"
                                    "\n"
                                    "machine test_pattern   # the board\n"
                                    "fault stuck U1.QC 0\n"
                                    "fault stuck U12.11 1\n"
                                    "fault crack U1.QA U13.14 30%\n"
                                    "fault crack R6.2 D5.A 37.25%\n",
                                    "t.bwp");
    REQUIRE(result.has_value());
    const Puzzle& p = *result;
    CHECK(p.file == "t.bwp");
    CHECK(p.machine == "test_pattern");
    CHECK(p.machineLine == 3);
    REQUIRE(p.faults.size() == 4);  // in file order
    CHECK(FaultAt<StuckAtDecl>(p, 0).line == 4);
    CHECK(FaultAt<StuckAtDecl>(p, 0).reference == "U1");
    CHECK(FaultAt<StuckAtDecl>(p, 0).pin == "QC");
    CHECK(FaultAt<StuckAtDecl>(p, 0).value == LogicValue::Zero);
    CHECK(FaultAt<StuckAtDecl>(p, 1).pin == "11");
    CHECK(FaultAt<StuckAtDecl>(p, 1).value == LogicValue::One);
    CHECK_FALSE(FaultAt<StuckAtDecl>(p, 0).input);
    CHECK(FaultAt<CrackDecl>(p, 2).line == 6);
    CHECK(FaultAt<CrackDecl>(p, 2).a.reference == "U1");
    CHECK(FaultAt<CrackDecl>(p, 2).a.pin == "QA");
    CHECK(FaultAt<CrackDecl>(p, 2).b.reference == "U13");
    CHECK(FaultAt<CrackDecl>(p, 2).b.pin == "14");
    CHECK(FaultAt<CrackDecl>(p, 2).percent == 30);
    CHECK(FaultAt<CrackDecl>(p, 3).a.reference == "R6");
    CHECK(FaultAt<CrackDecl>(p, 3).a.pin == "2");
    CHECK(FaultAt<CrackDecl>(p, 3).b.reference == "D5");
    CHECK(FaultAt<CrackDecl>(p, 3).b.pin == "A");
    CHECK(FaultAt<CrackDecl>(p, 3).percent == 37.25);
}

TEST_CASE("puzzle: Faults of several kinds in one list, in file order", "[machine]") {
    const auto result = ParsePuzzle("machine m\nfault joint U15.13\nfault stuck U1.QC 0\n"
                                    "fault switch SW1 open\nfault joint U15.12\n",
                                    "t.bwp");
    REQUIRE(result.has_value());
    REQUIRE(result->faults.size() == 4);
    CHECK(FaultAt<JointDecl>(*result, 0).pin.pin == "13");
    CHECK(FaultAt<StuckAtDecl>(*result, 1).pin == "QC");
    CHECK_FALSE(FaultAt<SwitchDecl>(*result, 2).closed);
    CHECK(FaultAt<JointDecl>(*result, 3).pin.pin == "12");
    CHECK(LineOf(result->faults[2]) == 4);
}

TEST_CASE("puzzle: errors", "[machine]") {
    CHECK(Errors("") == Lines{"0: no machine line"});
    CHECK(Errors("machine a\nmachine b\n") == Lines{"2: machine already given at line 1"});
    CHECK(Errors("machine\n") == Lines{"0: no machine line", "1: expected: machine NAME"});
    CHECK(Errors("machine ../x\n") == Lines{"0: no machine line", "1: bad machine name '../x'"});
    CHECK(Errors("machine m\nfoo\n") == Lines{"2: unknown directive 'foo'"});
    CHECK(Errors("machine m\nfault\nfault short U1.1\n") ==
          Lines{"2: expected: fault stuck|crack|bridge|joint|cell|column|switch ...",
                "3: expected: fault stuck|crack|bridge|joint|cell|column|switch ..."});
    CHECK(Errors("machine m\nfault stuck U1.QA\nfault stuck U1 0\nfault stuck U1.QA 2\n") ==
          Lines{"2: expected: fault stuck REF.PIN 0|1 [input]",
                "3: expected: fault stuck REF.PIN 0|1 [input]",
                "4: expected: fault stuck REF.PIN 0|1 [input]"});
    CHECK(Errors("machine m\nfault stuck U1.PB5 0 output\n") ==
          Lines{"2: expected: fault stuck REF.PIN 0|1 [input]"});
    const std::string expected = "expected: fault crack REF.PIN REF.PIN N%";
    CHECK(Errors("machine m\nfault crack F.Cu 52.30 41.00\nfault crack U1.QA U2.1 50\n"
                 "fault crack U1.QA U2 50%\nfault crack U1.QA 50%\n") ==
          Lines{"2: " + expected, "3: " + expected, "4: " + expected, "5: " + expected});
    CHECK(Errors("machine m\nfault crack U1.QA U2.1 101%\nfault crack U1.QA U2.1 1.234%\n"
                 "fault crack U1.QA U2.1 %\nfault crack U1.QA U2.1 5.%\n"
                 "fault crack U1.QA U2.1 -5%\n") ==
          Lines{"2: bad position '101%'", "3: bad position '1.234%'", "4: bad position '%'",
                "5: bad position '5.%'", "6: bad position '-5%'"});
    CHECK(Errors("machine m\nfault frob\n") ==
          Lines{"2: expected: fault stuck|crack|bridge|joint|cell|column|switch ..."});
    CHECK(Errors("machine m\nfault bridge U1.7\nfault bridge U17 U1.8\nfault bridge U1. U1.8\n") ==
          Lines{"2: expected: fault bridge REF.PIN REF.PIN",
                "3: expected: fault bridge REF.PIN REF.PIN",
                "4: expected: fault bridge REF.PIN REF.PIN"});
}

TEST_CASE("puzzle: joint", "[machine]") {
    const auto result =
        ParsePuzzle("machine m\nfault joint U15.13\nfault joint U15.2D  # name\n", "t.bwp");
    REQUIRE(result.has_value());
    REQUIRE(result->faults.size() == 2);
    CHECK(FaultAt<JointDecl>(*result, 0).line == 2);
    CHECK(FaultAt<JointDecl>(*result, 0).pin.reference == "U15");
    CHECK(FaultAt<JointDecl>(*result, 0).pin.pin == "13");
    CHECK(FaultAt<JointDecl>(*result, 1).pin.pin == "2D");
    CHECK(Errors("machine m\nfault joint\nfault joint U15\nfault joint U15.13 U15.12\n") ==
          Lines{"2: expected: fault joint REF.PIN", "3: expected: fault joint REF.PIN",
                "4: expected: fault joint REF.PIN"});
}

TEST_CASE("puzzle: bridge", "[machine]") {
    const auto result = ParsePuzzle("machine m\nfault bridge U1.7 U1.ENP  # same pad\n", "t.bwp");
    REQUIRE(result.has_value());
    REQUIRE(result->faults.size() == 1);
    const BridgeDecl& b = FaultAt<BridgeDecl>(*result, 0);
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

TEST_CASE("puzzle: switch", "[machine]") {
    const auto result =
        ParsePuzzle("machine m\nfault switch SW1 closed\nfault switch SW12 open\n", "t.bwp");
    REQUIRE(result.has_value());
    REQUIRE(result->faults.size() == 2);
    CHECK(FaultAt<SwitchDecl>(*result, 0).line == 2);
    CHECK(FaultAt<SwitchDecl>(*result, 0).reference == "SW1");
    CHECK(FaultAt<SwitchDecl>(*result, 0).closed);
    CHECK(FaultAt<SwitchDecl>(*result, 1).reference == "SW12");
    CHECK_FALSE(FaultAt<SwitchDecl>(*result, 1).closed);
    const std::string expected = "expected: fault switch REF closed|open";
    CHECK(Errors("machine m\nfault switch SW1\nfault switch SW1 shut\n"
                 "fault switch SW1 open now\n") ==
          Lines{"2: " + expected, "3: " + expected, "4: " + expected});
}

TEST_CASE("puzzle: cell and column", "[machine]") {
    const auto result = ParsePuzzle("machine m\nfault cell U3.IO2 $1fF 0\nfault cell U3.11 511 1\n"
                                    "fault column U15.O1 0  # inside\n",
                                    "t.bwp");
    REQUIRE(result.has_value());
    REQUIRE(result->faults.size() == 3);
    const CellDecl& first = FaultAt<CellDecl>(*result, 0);
    CHECK(first.line == 2);
    CHECK(first.pin.reference == "U3");
    CHECK(first.pin.pin == "IO2");
    CHECK(first.word == 0x1FFu);
    CHECK(first.value == LogicValue::Zero);
    CHECK(FaultAt<CellDecl>(*result, 1).pin.pin == "11");
    CHECK(FaultAt<CellDecl>(*result, 1).word == 511u);
    CHECK(FaultAt<CellDecl>(*result, 1).value == LogicValue::One);
    CHECK(FaultAt<CellDecl>(*result, 2).pin.reference == "U15");
    CHECK_FALSE(FaultAt<CellDecl>(*result, 2).word.has_value());
    CHECK(FaultAt<CellDecl>(*result, 0).memory.empty());
    const std::string cell = "expected: fault cell REF.PIN [MEMORY] WORD 0|1";
    const std::string column = "expected: fault column REF.PIN [MEMORY] 0|1";
    CHECK(Errors("machine m\nfault cell U3.IO2 $1FF\nfault cell U3 $1FF 0\n"
                 "fault cell U3.IO2 $1FF 2\nfault column U3.IO2 $1FF 0\nfault column U3.IO2\n") ==
          Lines{"2: " + cell, "3: " + cell, "4: " + cell, "5: " + column, "6: " + column});
    CHECK(Errors("machine m\nfault cell U3.IO2 $ 0\nfault cell U3.IO2 $1G 0\n"
                 "fault cell U3.IO2 12a 0\nfault cell U3.IO2 $123456789 0\n"
                 "fault cell U3.IO2 1234567890 0\n") ==
          Lines{"2: bad word '$'", "3: bad word '$1G'", "4: bad word '12a'",
                "5: bad word '$123456789'", "6: bad word '1234567890'"});
}

TEST_CASE("puzzle: cell and column in a named memory", "[machine]") {
    const auto result =
        ParsePuzzle("machine m\nfault cell U2.D3 ram $3F 0\nfault column U2.D0 rom 1\n", "t.bwp");
    REQUIRE(result.has_value());
    REQUIRE(result->faults.size() == 2);
    CHECK(FaultAt<CellDecl>(*result, 0).memory == "ram");
    CHECK(FaultAt<CellDecl>(*result, 0).word == 0x3Fu);
    CHECK(FaultAt<CellDecl>(*result, 0).value == LogicValue::Zero);
    CHECK(FaultAt<CellDecl>(*result, 1).memory == "rom");
    CHECK_FALSE(FaultAt<CellDecl>(*result, 1).word.has_value());
    CHECK(FaultAt<CellDecl>(*result, 1).value == LogicValue::One);
    const std::string cell = "expected: fault cell REF.PIN [MEMORY] WORD 0|1";
    const std::string column = "expected: fault column REF.PIN [MEMORY] 0|1";
    CHECK(Errors("machine m\nfault cell U2.D3 Ram $3F 0\nfault cell U2.D3 r4m $3F 0\n"
                 "fault cell U2.D3 ram x $3F 0\nfault column U2.D0 ROM 1\n"
                 "fault column U2.D0 rom x 1\nfault cell U2.D3 ram $3G 0\n") ==
          Lines{"2: " + cell, "3: " + cell, "4: " + cell, "5: " + column, "6: " + column,
                "7: bad word '$3G'"});
}

TEST_CASE("puzzle: a stuck port pin read only", "[machine]") {
    const auto result = ParsePuzzle("machine m\nfault stuck UA5.PB5 0 input\n", "t.bwp");
    REQUIRE(result.has_value());
    REQUIRE(result->faults.size() == 1);
    CHECK(FaultAt<StuckAtDecl>(*result, 0).pin == "PB5");
    CHECK(FaultAt<StuckAtDecl>(*result, 0).value == LogicValue::Zero);
    CHECK(FaultAt<StuckAtDecl>(*result, 0).input);
}

TEST_CASE("puzzle: FormatFault of each kind", "[machine]") {
    using enum LogicValue;
    CHECK(FormatFault(StuckAtDecl{3, "U5", "QD", Zero}) == "fault stuck U5.QD 0");
    CHECK(FormatFault(StuckAtDecl{0, "UA5", "PB5", One, true}) == "fault stuck UA5.PB5 1 input");
    CHECK(FormatFault(CrackDecl{0, {"R6", "2"}, {"D5", "2"}, 51}) == "fault crack R6.2 D5.2 51%");
    CHECK(FormatFault(CrackDecl{0, {"U1", "QA"}, {"U13", "2B"}, 7.5}) ==
          "fault crack U1.QA U13.2B 7.5%");
    CHECK(FormatFault(CrackDecl{0, {"U1", "1"}, {"U2", "1"}, 33.33}) ==
          "fault crack U1.1 U2.1 33.33%");
    CHECK(FormatFault(CrackDecl{0, {"U1", "1"}, {"U2", "1"}, 0}) == "fault crack U1.1 U2.1 0%");
    CHECK(FormatFault(CrackDecl{0, {"U1", "1"}, {"U2", "1"}, 100}) == "fault crack U1.1 U2.1 100%");
    CHECK(FormatFault(BridgeDecl{0, {"U5", "3"}, {"U5", "4"}}) == "fault bridge U5.3 U5.4");
    CHECK(FormatFault(JointDecl{0, {"U15", "13"}}) == "fault joint U15.13");
    CHECK(FormatFault(CellDecl{0, {"U3", "IO2"}, 0x1FF, Zero}) == "fault cell U3.IO2 $1FF 0");
    CHECK(FormatFault(CellDecl{0, {"U2", "D3"}, 0x3F, One, "ram"}) == "fault cell U2.D3 ram $3F 1");
    CHECK(FormatFault(CellDecl{0, {"U15", "O1"}, std::nullopt, Zero}) == "fault column U15.O1 0");
    CHECK(FormatFault(CellDecl{0, {"U2", "D0"}, std::nullopt, One, "rom"}) ==
          "fault column U2.D0 rom 1");
    CHECK(FormatFault(SwitchDecl{0, "SW1", true}) == "fault switch SW1 closed");
    CHECK(FormatFault(SwitchDecl{0, "SW1", false}) == "fault switch SW1 open");
}

TEST_CASE("puzzle: every Puzzle's faults format back", "[machine]") {
    // Each Authored Puzzle's Fault, written by FormatFault and read by ParsePuzzle, is the same
    // Fault: the same kind, written the same again.
    const MachineList list = ListMachines(test::RootDir() / "machines");
    std::size_t faults = 0;
    for (const MachineEntry& machine : list.machines) {
        for (const PuzzleEntry& entry : machine.puzzles) {
            INFO(entry.file.filename().string());
            const auto puzzle = ReadPuzzleFile(entry.file);
            REQUIRE(puzzle.has_value());
            for (const FaultDecl& fault : puzzle->faults) {
                const std::string line = FormatFault(fault);
                INFO(line);
                const auto again =
                    ParsePuzzle("machine " + puzzle->machine + "\n" + line + "\n", "again.bwp");
                REQUIRE(again.has_value());
                REQUIRE(again->faults.size() == 1);
                CHECK(again->faults[0].index() == fault.index());
                CHECK(FormatFault(again->faults[0]) == line);
                ++faults;
            }
        }
    }
    CHECK(faults == 121);
}
