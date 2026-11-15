#include "parts/catalogue.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/rom.hpp"

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
        if (part.name == "POR") {
            params.push_back({"time", "1ms"});
        }
        if (part.name == "6530") {
            params.push_back({"mask", "002"});
        }
        if (const RomSpec* rom = FindRomSpec(part.name); rom != nullptr && rom->selectable) {
            params.push_back({"select", std::string(rom->enables.size(), 'L')});
        }
        if (FindRomSpec(part.name) != nullptr || part.name == "6530") {
            params.push_back({"contents", std::string(BW_ROOT_DIR) + "/tests/data/empty.rom"});
        }
        const auto created = part.create(circuit, "U1", params);
        INFO((created ? std::string() : created.error()));
        REQUIRE(created.has_value());
        const auto pins = (*created)->pins();
        REQUIRE(part.package.starts_with("DIP-"));
        const std::size_t count = std::stoul(std::string(part.package.substr(4)));
        REQUIRE(pins.size() == count);

        std::set<std::string> names;
        for (const PinSpec& pin : pins) {
            names.insert(pin.name);
            // MOS chips' signal pins (bus charge, X read as a random bit).
            constexpr std::string_view kMosSignals[] = {"2114", "2518", "2524", "6102", "6502",
                                                        "6520", "6522", "6530", "Z80"};
            const bool mos = std::ranges::find(kMosSignals, part.name) != std::end(kMosSignals) &&
                             pin.kind != PinKind::Power;
            CHECK(pin.family == (mos ? Family::Mos : Family::Ttl));
            if (pin.name == "GND" || pin.name == "GND2" || pin.name == "VCC") {
                CHECK(pin.kind == PinKind::Power);
            } else {
                CHECK(pin.kind != PinKind::Power);
            }
        }
        CHECK(names.size() == count);
        // P-MOS chips between VCC and a negative supply (not connected): no GND pin.
        constexpr std::string_view kNoGround[] = {"2518", "2524"};
        const bool noGround = std::ranges::find(kNoGround, part.name) != std::end(kNoGround);
        CHECK(names.contains("GND") != noGround);
        CHECK(names.contains("VCC"));
        // The 74LS90's datasheet puts VCC on pin 5 and GND on pin 10; the 2513's GND is pin 10; the
        // Z80's VCC is pin 11, GND pin 29; the 6502's GND is pin 1, VCC pin 8, GND2 pin 21; the
        // 6102's GND is pin 9, VCC pin 10; the 6530's GND is pin 1, VCC pin 20; the 74100's GND
        // is pin 7, VCC pin 24.
        constexpr std::string_view kOffCornerPower[] = {"74LS90", "74LS93", "2513",   "6102",
                                                        "6502",   "6520",   "6522",   "6530",
                                                        "74100",  "Z80",    "Z80STUB"};
        if (std::ranges::find(kOffCornerPower, part.name) == std::end(kOffCornerPower)) {
            CHECK((noGround || pins[count / 2 - 1].name == "GND"));
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
            CHECK(names.contains(lamp.cathodes[0]));
            CHECK((lamp.cathodes[1].empty() || names.contains(lamp.cathodes[1])));
            CHECK(names.contains(lamp.anodes[0]));
            CHECK((lamp.anodes[1].empty() || names.contains(lamp.anodes[1])));
        }
    }
}
