#include "parts/catalogue.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("catalogue: FindPart known and unknown", "[parts]") {
    const PartInfo* part = FindPart("74LS00");
    REQUIRE(part != nullptr);
    CHECK(part->name == "74LS00");
    CHECK(part->package == "DIP-14");
    CHECK(part->kind == PartKind::Chip);
    CHECK(FindPart("74LS999") == nullptr);
    CHECK(FindPart("") == nullptr);
}

TEST_CASE("catalogue: chip pinouts are consistent", "[parts]") {
    for (const PartInfo& part : AllParts()) {
        if (part.kind != PartKind::Chip) {
            continue;
        }
        INFO(part.name);
        Circuit circuit;
        std::vector<PartParam> params;
        if (part.name == "OSC") {
            params.push_back({"freq", "1MHz"});
        }
        const auto created = part.create(circuit, "U1", params);
        REQUIRE(created.has_value());
        const auto pins = (*created)->pins();
        const std::size_t count = part.package == "DIP-16" ? 16 : 14;
        REQUIRE(part.package == (count == 16 ? "DIP-16" : "DIP-14"));
        REQUIRE(pins.size() == count);

        std::set<std::string> names;
        for (const PinSpec& pin : pins) {
            names.insert(pin.name);
            CHECK(pin.family == Family::Ttl);
            if (pin.name == "GND" || pin.name == "VCC") {
                CHECK(pin.kind == PinKind::Power);
            } else {
                CHECK(pin.kind != PinKind::Power);
            }
        }
        CHECK(names.size() == count);
        CHECK(names.contains("GND"));
        CHECK(names.contains("VCC"));
        // The 74LS90's datasheet puts VCC on pin 5 and GND on pin 10.
        constexpr std::string_view kOffCornerPower[] = {"74LS90"};
        if (std::ranges::find(kOffCornerPower, part.name) == std::end(kOffCornerPower)) {
            CHECK(pins[count / 2 - 1].name == "GND");
            CHECK(pins[count - 1].name == "VCC");
        }
    }
}

TEST_CASE("catalogue: parameters rejected", "[parts]") {
    Circuit circuit;
    const std::vector<PartParam> params{{"freq", "1MHz"}};
    const auto created = FindPart("74LS04")->create(circuit, "U1", params);
    REQUIRE_FALSE(created.has_value());
    CHECK(created.error() == "74LS04 takes no parameters");
}

TEST_CASE("catalogue: every lamp names Pins of its Part", "[parts]") {
    for (const PartInfo& part : AllParts()) {
        if (part.lamps.empty()) {
            continue;
        }
        INFO(part.name);
        Circuit circuit;
        const auto created = part.create(circuit, "D1", {});
        REQUIRE(created.has_value());
        std::set<std::string, std::less<>> names;
        for (const PinSpec& pin : (*created)->pins()) {
            names.insert(pin.name);
        }
        for (const Lamp& lamp : part.lamps) {
            CHECK(names.contains(lamp.cathode));
            CHECK(names.contains(lamp.anodes[0]));
            CHECK((lamp.anodes[1].empty() || names.contains(lamp.anodes[1])));
        }
    }
}
