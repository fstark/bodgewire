#include "board/sexpr.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("sexpr: atoms, strings and nesting", "[board]") {
    const auto root = ParseSExpr("(kicad_pcb (version 20241229)\n  (net 1 \"A \\\"B\\\" \\\\\")\n)",
                                 "t.kicad_pcb");
    REQUIRE(root.has_value());
    CHECK(root->head() == "kicad_pcb");
    CHECK(root->line == 1);
    REQUIRE(root->list.size() == 3);
    CHECK(root->find("version")->atomAt(1) == "20241229");
    const SExpr* net = root->find("net");
    REQUIRE(net != nullptr);
    CHECK(net->line == 2);
    CHECK(net->atomAt(1) == "1");
    CHECK(net->atomAt(2) == "A \"B\" \\");
    CHECK(net->atomAt(3).empty());
    CHECK(root->find("missing") == nullptr);
}

TEST_CASE("sexpr: empty string and adjacent tokens", "[board]") {
    const auto root = ParseSExpr("(a\"\"(b)c)", "t");
    REQUIRE(root.has_value());
    REQUIRE(root->list.size() == 4);
    CHECK(root->list[1].atom.empty());
    CHECK_FALSE(root->list[1].isList);
    CHECK(root->list[2].head() == "b");
    CHECK(root->list[3].atom == "c");
}

TEST_CASE("sexpr: errors", "[board]") {
    const auto unbalanced = ParseSExpr("(a\n(b)", "t");
    REQUIRE_FALSE(unbalanced.has_value());
    CHECK(Format(unbalanced.error()) == "t:2: error: unbalanced parentheses");

    const auto unterminated = ParseSExpr("(a \"b)", "t");
    REQUIRE_FALSE(unterminated.has_value());
    CHECK(unterminated.error().message == "unterminated string");

    CHECK_FALSE(ParseSExpr("a", "t").has_value());
    CHECK_FALSE(ParseSExpr("(a) (b)", "t").has_value());
    CHECK_FALSE(ParseSExpr("", "t").has_value());
}
