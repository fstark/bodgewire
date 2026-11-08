#include "parts/digit.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"
#include "sim/circuit.hpp"

using namespace bw;

TEST_CASE("digit: catalogue entry", "[parts]") {
    const PartInfo* info = FindPart("DIGIT");
    REQUIRE(info != nullptr);
    CHECK(info->kind == PartKind::Led);
    CHECK(info->package == "DIGIT-0.56");
    Circuit circuit;
    const std::vector<PartParam> params{{"colour", "red"}};
    const auto created = info->create(circuit, "DS1", params);
    REQUIRE_FALSE(created.has_value());
    CHECK(created.error() == "DIGIT takes no parameters");
}

TEST_CASE("digit: pins in pad order are passive loads", "[parts]") {
    const Digit digit("DS1");
    std::vector<std::string> names;
    for (const PinSpec& pin : digit.pins()) {
        names.push_back(pin.name);
        CHECK(pin.kind == PinKind::Input);
        CHECK(pin.family == Family::Mos);
    }
    CHECK(names == std::vector<std::string>{"E", "D", "CA1", "C", "DP", "B", "A", "CA2", "F", "G"});
}

TEST_CASE("digit: seven segments and a point on both anodes", "[parts]") {
    const auto lamps = FindPart("DIGIT")->lamps;
    REQUIRE(lamps.size() == 8);
    const std::string cathodes = "ABCDEFG";
    for (std::size_t k = 0; k < 7; ++k) {
        CHECK(lamps[k].shape == static_cast<LampShape>(k + 1));
        CHECK(lamps[k].cathode == std::string(1, cathodes[k]));
    }
    CHECK(lamps[7].shape == LampShape::DecimalPoint);
    CHECK(lamps[7].cathode == "DP");
    for (const Lamp& lamp : lamps) {
        CHECK(lamp.anodes[0] == "CA1");
        CHECK(lamp.anodes[1] == "CA2");
    }
    const auto led = FindPart("LED")->lamps;
    REQUIRE(led.size() == 1);
    CHECK(led[0].shape == LampShape::Round);
    CHECK(led[0].anodes[0] == "A");
    CHECK(led[0].anodes[1].empty());
    CHECK(led[0].cathode == "K");
}
