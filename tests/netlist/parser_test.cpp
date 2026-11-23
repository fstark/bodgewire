#include "netlist/parser.hpp"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

// The single error for `text`, as "line: message".
std::string OnlyError(std::string_view text) {
    const auto result = ParseNetlist(text, "t.bwn");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().size() == 1);
    const Diagnostic& d = result.error()[0];
    CHECK(d.file == "t.bwn");
    CHECK(d.severity == Severity::Error);
    return std::to_string(d.line) + ": " + d.message;
}

}  // namespace

TEST_CASE("parser: parts, params and nets", "[netlist]") {
    const auto result = ParseNetlist("part X1 OSC freq=1MHz\n"
                                     "part U1 74LS161\n"
                                     "\n"
                                     "net CLK X1.OUT U1.2\n"
                                     "net CLK_2 U1.QA\n",
                                     "t.bwn");
    REQUIRE(result.has_value());
    const Netlist& n = *result;
    CHECK(n.file == "t.bwn");
    REQUIRE(n.parts.size() == 2);
    CHECK(n.parts[0].line == 1);
    CHECK(n.parts[0].reference == "X1");
    CHECK(n.parts[0].part == "OSC");
    REQUIRE(n.parts[0].params.size() == 1);
    CHECK(n.parts[0].params[0].name == "freq");
    CHECK(n.parts[0].params[0].value == "1MHz");
    CHECK(n.parts[1].line == 2);
    CHECK(n.parts[1].params.empty());
    REQUIRE(n.nets.size() == 2);
    CHECK(n.nets[0].line == 4);
    CHECK(n.nets[0].name == "CLK");
    REQUIRE(n.nets[0].pins.size() == 2);
    CHECK(n.nets[0].pins[0].reference == "X1");
    CHECK(n.nets[0].pins[0].pin == "OUT");
    CHECK(n.nets[0].pins[1].reference == "U1");
    CHECK(n.nets[0].pins[1].pin == "2");
    CHECK(n.nets[1].line == 5);
    CHECK(n.nets[1].name == "CLK_2");
}

TEST_CASE("parser: comments and blank lines", "[netlist]") {
    const auto result = ParseNetlist("# header\n"
                                     "\n"
                                     "   \t  \n"
                                     "part\tU1   74LS00 # a NAND\n"
                                     "net A U1.1#no space before comment\r\n"
                                     "# part U2 74LS00\n",
                                     "t.bwn");
    REQUIRE(result.has_value());
    REQUIRE(result->parts.size() == 1);
    CHECK(result->parts[0].line == 4);
    CHECK(result->parts[0].part == "74LS00");
    REQUIRE(result->nets.size() == 1);
    CHECK(result->nets[0].line == 5);
    REQUIRE(result->nets[0].pins.size() == 1);
    CHECK(result->nets[0].pins[0].pin == "1");
}

TEST_CASE("parser: unknown statement", "[netlist]") {
    CHECK(OnlyError("\nxyz U1\n") == "2: unknown statement 'xyz'");
}

TEST_CASE("parser: part with too few tokens", "[netlist]") {
    CHECK(OnlyError("part U1\n") == "1: part: expected 'part <ref> <part> [key=value ...]'");
}

TEST_CASE("parser: bad reference", "[netlist]") {
    CHECK(OnlyError("part u1 74LS00") == "1: bad reference 'u1': letters then digits, e.g. U1");
    CHECK(OnlyError("part U 74LS00") == "1: bad reference 'U': letters then digits, e.g. U1");
    CHECK(OnlyError("part 1U 74LS00") == "1: bad reference '1U': letters then digits, e.g. U1");
}

TEST_CASE("parser: bad part name", "[netlist]") {
    CHECK(OnlyError("part U1 74-LS00") == "1: bad part name '74-LS00'");
}

TEST_CASE("parser: bad parameter", "[netlist]") {
    CHECK(OnlyError("part X1 OSC freq") == "1: bad parameter 'freq': expected key=value");
    CHECK(OnlyError("part X1 OSC freq=") == "1: bad parameter 'freq=': expected key=value");
    CHECK(OnlyError("part X1 OSC =1MHz") == "1: bad parameter '=1MHz': expected key=value");
}

TEST_CASE("parser: net with too few tokens", "[netlist]") {
    CHECK(OnlyError("net CLK") == "1: net: expected 'net <name> <ref>.<pin> ...'");
}

TEST_CASE("parser: bad net name", "[netlist]") {
    CHECK(OnlyError("net 1CLK U1.1") == "1: bad net name '1CLK'");
    CHECK(OnlyError("net CLK-A U1.1") == "1: bad net name 'CLK-A'");
}

TEST_CASE("parser: bad pin reference", "[netlist]") {
    CHECK(OnlyError("net CLK U1") == "1: bad pin reference 'U1': expected <ref>.<pin>");
    CHECK(OnlyError("net CLK .1") == "1: bad pin reference '.1': expected <ref>.<pin>");
    CHECK(OnlyError("net CLK U1.") == "1: bad pin reference 'U1.': expected <ref>.<pin>");
    CHECK(OnlyError("net CLK U1.1.2") == "1: bad pin reference 'U1.1.2': expected <ref>.<pin>");
}

TEST_CASE("parser: reports all errors", "[netlist]") {
    const auto result = ParseNetlist("part u1 74LS00\n"
                                     "part U2 74LS00\n"
                                     "wire A B\n"
                                     "net 9 U2.1\n",
                                     "t.bwn");
    REQUIRE_FALSE(result.has_value());
    const auto& errors = result.error();
    REQUIRE(errors.size() == 3);
    CHECK(errors[0].line == 1);
    CHECK(errors[1].line == 3);
    CHECK(errors[2].line == 4);
}

TEST_CASE("parser: ParsePinRef splits REF.PIN at its one dot", "[netlist]") {
    const auto number = ParsePinRef("U3.7");
    REQUIRE(number.has_value());
    CHECK(number->reference == "U3");
    CHECK(number->pin == "7");
    const auto name = ParsePinRef("U12.CLR_N");
    REQUIRE(name.has_value());
    CHECK(name->reference == "U12");
    CHECK(name->pin == "CLR_N");
    for (const char* bad : {"", "U3", ".7", "U3.", "U3.7.1", "."}) {
        CHECK_FALSE(ParsePinRef(bad).has_value());
    }
}

TEST_CASE("parser: IsReference is letters then digits", "[netlist]") {
    for (const char* good : {"U1", "SW12", "RN3"}) {
        CHECK(IsReference(good));
    }
    for (const char* bad : {"", "U", "12", "u1", "U1A", "U-1", "1U"}) {
        CHECK_FALSE(IsReference(bad));
    }
}
