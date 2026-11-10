#include "board/place_rules.hpp"

#include <string>
#include <variant>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

std::vector<std::string> Errors(std::string_view text) {
    const auto rules = ParsePlaceRules(text, "t.place");
    REQUIRE_FALSE(rules.has_value());
    std::vector<std::string> messages;
    for (const Diagnostic& d : rules.error()) {
        messages.push_back(Format(d));
    }
    return messages;
}

}  // namespace

TEST_CASE("place rules: edge, row, next and grid", "[board]") {
    const auto rules = ParsePlaceRules("# panel\n"
                                       "edge J1 left middle   # power\n"
                                       "\n"
                                       "edge J2 back right\n"
                                       "row front 30% D1 D2 D3\n"
                                       "next R1 D1 back\n"
                                       "edge J3 right front\n"
                                       "grid 12 5\n",
                                       "t.place");
    REQUIRE(rules.has_value());
    REQUIRE(rules->rules.size() == 5);

    const auto& j1 = std::get<EdgeRule>(rules->rules[0].rule);
    CHECK(rules->rules[0].line == 2);
    CHECK(j1.edge == Edge::Left);
    CHECK(j1.at == 0.5);
    CHECK(j1.references == std::vector<std::string>{"J1"});

    const auto& j2 = std::get<EdgeRule>(rules->rules[1].rule);
    CHECK(j2.edge == Edge::Back);
    CHECK(j2.at == 1.0);

    const auto& row = std::get<EdgeRule>(rules->rules[2].rule);
    CHECK(row.edge == Edge::Front);
    CHECK(row.at == 0.3);
    CHECK(row.references == std::vector<std::string>{"D1", "D2", "D3"});

    const auto& next = std::get<NextRule>(rules->rules[3].rule);
    CHECK(next.reference == "R1");
    CHECK(next.other == "D1");
    CHECK(next.side == Edge::Back);

    CHECK(std::get<EdgeRule>(rules->rules[4].rule).at == 1.0);
    CHECK(rules->grid == std::pair<std::int32_t, std::int32_t>{12, 5});
}

TEST_CASE("place rules: ends of each edge", "[board]") {
    const auto rules =
        ParsePlaceRules("edge A front left\nedge B left back\nedge C back 0%\n", "t.place");
    REQUIRE(rules.has_value());
    CHECK(std::get<EdgeRule>(rules->rules[0].rule).at == 0.0);
    CHECK(std::get<EdgeRule>(rules->rules[1].rule).at == 0.0);
    CHECK(std::get<EdgeRule>(rules->rules[2].rule).at == 0.0);
}

TEST_CASE("place rules: errors name their line", "[board]") {
    CHECK(Errors("put J1 left\n") ==
          std::vector<std::string>{"t.place:1: error: unknown rule 'put'"});
    CHECK(Errors("edge J1 left\n") ==
          std::vector<std::string>{"t.place:1: error: expected: edge REF EDGE AT"});
    CHECK(Errors("\nedge J1 top middle\n") ==
          std::vector<std::string>{"t.place:2: error: unknown edge 'top'"});
    CHECK(Errors("edge J1 left right\n") ==
          std::vector<std::string>{"t.place:1: error: 'right' is not an end of the left edge"});
    CHECK(Errors("edge J1 front 120%\n") ==
          std::vector<std::string>{
              "t.place:1: error: bad position '120%': an end, middle or 0%-100%"});
    CHECK(Errors("edge J1 front middle\nrow back left J2 J1\n") ==
          std::vector<std::string>{"t.place:2: error: 'J1' has a rule already"});
    CHECK(Errors("next R1 SW1 right\nedge SW1 front middle\n") ==
          std::vector<std::string>{"t.place:1: error: 'SW1' is not placed by an earlier rule"});
    CHECK(Errors("edge SW1 front middle\nnext R1 SW1 up\n") ==
          std::vector<std::string>{"t.place:2: error: unknown side 'up'"});
    CHECK(Errors("grid 12 5\ngrid 0 5\n") ==
          std::vector<std::string>{"t.place:2: error: grid given twice"});
    CHECK(Errors("grid 0 5\n") == std::vector<std::string>{"t.place:1: error: bad grid size '0'"});
    CHECK(Errors("row front middle\nput\n").size() == 2);
}
