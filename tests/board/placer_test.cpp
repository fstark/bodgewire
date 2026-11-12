#include "board/placer.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"

using namespace bw;

namespace {

constexpr std::int64_t kMm = 1'000'000;
constexpr std::int64_t kSnap = 1'270'000;

const FootprintLibrary& Footprints() {
    static const FootprintLibrary library = [] {
        std::vector<std::string_view> packages;
        for (const PartInfo& part : AllParts()) {
            packages.push_back(part.package);
        }
        auto loaded = LoadFootprints(BW_FOOTPRINT_DIR, packages);
        REQUIRE(loaded.has_value());
        return std::move(*loaded);
    }();
    return library;
}

std::unique_ptr<LoadedCircuit> Circuit(std::string_view netlist) {
    auto loaded = LoadNetlist(netlist, "t.bwn");
    if (!loaded) {
        for (const Diagnostic& d : loaded.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(loaded.has_value());
    return std::move(*loaded);
}

std::expected<Placement, std::vector<Diagnostic>>
Place(std::string_view netlist, std::string_view rules, PlaceOptions options = {.tries = 1}) {
    const auto circuit = Circuit(netlist);
    const auto parsed = ParsePlaceRules(rules, "t.place");
    REQUIRE(parsed.has_value());
    return PlaceBoard(*circuit, *parsed, "t.place", Footprints(), options);
}

Placement Placed(std::string_view netlist, std::string_view rules) {
    auto placement = Place(netlist, rules);
    if (!placement) {
        for (const Diagnostic& d : placement.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(placement.has_value());
    return std::move(*placement);
}

std::vector<std::string> Errors(std::string_view netlist, std::string_view rules) {
    const auto placement = Place(netlist, rules);
    REQUIRE_FALSE(placement.has_value());
    std::vector<std::string> messages;
    for (const Diagnostic& d : placement.error()) {
        messages.push_back(Format(d));
    }
    return messages;
}

struct Box {
    Point min, max;
};

const PlacedPart& Part(const Placement& placement, std::string_view reference) {
    for (const PlacedPart& part : placement.parts) {
        if (part.reference == reference) {
            return part;
        }
    }
    FAIL("no part " << reference);
    throw;
}

Box Courtyard(const Placement& placement, std::string_view reference) {
    const PlacedPart& part = Part(placement, reference);
    const Footprint& fp = *part.footprint;
    return {{part.origin.x + fp.courtyardMin.x, part.origin.y + fp.courtyardMin.y},
            {part.origin.x + fp.courtyardMax.x, part.origin.y + fp.courtyardMax.y}};
}

bool Overlap(const Box& a, const Box& b) {
    return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
}

bool Inside(const Box& inner, Point min, Point max) {
    return inner.min.x >= min.x && inner.min.y >= min.y && inner.max.x <= max.x &&
           inner.max.y <= max.y;
}

constexpr std::string_view kLeds = "part J1 POWER\n"
                                   "part U1 74LS04\n"
                                   "part D1 LED\n"
                                   "part D2 LED\n"
                                   "part D3 LED\n"
                                   "part R1 R value=330\n"
                                   "net L1 U1.1Y D1.K\n"
                                   "net L2 U1.2Y D2.K\n"
                                   "net L3 U1.3Y D3.K\n"
                                   "net A1 R1.2 D1.A\n"
                                   "net VCC R1.1 D2.A D3.A\n";

}  // namespace

TEST_CASE("placer: an edge part lies against its edge at its position", "[board]") {
    const Placement p = Placed(kLeds, "edge J1 left middle\n"
                                      "edge D1 front 30%\n"
                                      "edge D2 front right\n"
                                      "edge D3 back left\n");
    const Box d1 = Courtyard(p, "D1");
    CHECK(d1.max.y <= p.outlineMax.y - kMm);
    CHECK(d1.max.y > p.outlineMax.y - kMm - kSnap);
    const double width = static_cast<double>(p.outlineMax.x - p.outlineMin.x - 2 * kMm);
    const double centre = static_cast<double>(d1.min.x + d1.max.x) / 2;
    CHECK(std::abs(centre - static_cast<double>(p.outlineMin.x + kMm) - 0.3 * width) <=
          kSnap / 2 + 1);
    const Box d2 = Courtyard(p, "D2");
    CHECK(d2.max.x <= p.outlineMax.x - kMm);
    CHECK(d2.max.x > p.outlineMax.x - kMm - kSnap);
    const Box d3 = Courtyard(p, "D3");
    CHECK(d3.min.y >= p.outlineMin.y + kMm);
    CHECK(d3.min.x >= p.outlineMin.x + kMm);
    CHECK(d3.min.x < p.outlineMin.x + kMm + kSnap);
    const Box j1 = Courtyard(p, "J1");
    CHECK(j1.min.x >= p.outlineMin.x + kMm);
    CHECK(j1.min.x < p.outlineMin.x + kMm + kSnap);
}

TEST_CASE("placer: a DIP-24 needs a rule, then sits in its band", "[board]") {
    const std::string netlist =
        "part U1 74LS04\npart U2 2716 contents=" + std::string(BW_ROOT_DIR) +
        "/tests/data/empty.rom\n";
    CHECK(Errors(netlist, "") ==
          std::vector<std::string>{"t.place: error: 'U2' (DIP-24) needs a placement rule"});
    const Placement p = Placed(netlist, "row front middle U2\n");
    const Box u2 = Courtyard(p, "U2");
    CHECK(u2.max.y <= p.outlineMax.y - kMm);
    CHECK(u2.max.y > p.outlineMax.y - kMm - kSnap);
    CHECK_FALSE(Overlap(u2, Courtyard(p, "U1")));
}

TEST_CASE("placer: DIP-18 and DIP-20 sit in the chip grid", "[board]") {
    const Placement p =
        Placed("part U1 74LS04\npart U2 2114\npart U3 74LS244\npart U4 74LS161\n", "");
    for (const char* reference : {"U1", "U2", "U3", "U4"}) {
        INFO(reference);
        CHECK(Inside(Courtyard(p, reference), p.outlineMin, p.outlineMax));
    }
    CHECK_FALSE(Overlap(Courtyard(p, "U2"), Courtyard(p, "U3")));
    CHECK_FALSE(Overlap(Courtyard(p, "U1"), Courtyard(p, "U3")));
    CHECK_FALSE(Overlap(Courtyard(p, "U3"), Courtyard(p, "U4")));
}

TEST_CASE("placer: DIP-8 sits in the chip grid", "[board]") {
    const Placement p = Placed("part U1 74LS04\npart U2 2524\npart U3 74LS161\n", "");
    for (const char* reference : {"U1", "U2", "U3"}) {
        INFO(reference);
        CHECK(Inside(Courtyard(p, reference), p.outlineMin, p.outlineMax));
    }
    CHECK_FALSE(Overlap(Courtyard(p, "U1"), Courtyard(p, "U2")));
    CHECK_FALSE(Overlap(Courtyard(p, "U2"), Courtyard(p, "U3")));
}

TEST_CASE("placer: a row keeps its order with gaps between", "[board]") {
    const Placement p = Placed(kLeds, "edge J1 left middle\nrow front middle D1 D2 D3\n");
    const Box d1 = Courtyard(p, "D1"), d2 = Courtyard(p, "D2"), d3 = Courtyard(p, "D3");
    CHECK(d2.min.x - d1.max.x >= 2 * kMm - kSnap);
    CHECK(d3.min.x - d2.max.x >= 2 * kMm - kSnap);
    CHECK(d1.max.y == d2.max.y);
    CHECK(d2.max.y == d3.max.y);
}

TEST_CASE("placer: next puts a part beside another", "[board]") {
    const Placement p = Placed(kLeds, "edge J1 left middle\n"
                                      "row front middle D1 D2 D3\n"
                                      "next R1 D1 back\n");
    const Box d1 = Courtyard(p, "D1"), r1 = Courtyard(p, "R1");
    CHECK(r1.max.y <= d1.min.y);
    CHECK(d1.min.y - r1.max.y <= 2 * kMm + kSnap);
    CHECK(std::abs((r1.min.x + r1.max.x) / 2 - (d1.min.x + d1.max.x) / 2) <= kSnap);
    for (const PlacedPart& part : p.parts) {
        CHECK(Inside(Courtyard(p, part.reference), p.outlineMin, p.outlineMax));
    }
    CHECK_FALSE(Overlap(r1, Courtyard(p, "U1")));
}

TEST_CASE("placer: refusals", "[board]") {
    CHECK(Errors(kLeds, "edge J1 left middle\nrow front middle D1 D2 D3\nnext R1 D1 front\n") ==
          std::vector<std::string>{"t.place:3: error: 'R1' falls off the front edge"});
    CHECK(Errors(kLeds, "edge J1 left middle\nrow front middle D1 D2\nedge D3 front middle\n"
                        "next R1 D1 back\n") ==
          std::vector<std::string>{"t.place: error: 'D1' overlaps 'D3'",
                                   "t.place: error: 'D2' overlaps 'D3'"});
    CHECK(Errors("part U1 74LS04\npart DS1 DIGIT\n", "") ==
          std::vector<std::string>{"t.place: error: 'DS1' (DIGIT-0.56) needs a placement rule"});
    CHECK(Errors("part U1 74LS04\n", "edge J9 left middle\n") ==
          std::vector<std::string>{"t.place:1: error: unknown reference 'J9'"});
    CHECK(Errors("part U1 74LS04\npart U2 74LS04\n", "grid 1 1\n") ==
          std::vector<std::string>{"t.place: error: grid 1 1 has room for 1 parts, not 2"});
    CHECK(Errors(kLeds, "edge J1 front middle\nrow left middle D1 D2 D3\nnext R1 D1 right\n") ==
          std::vector<std::string>{"t.place:2: error: 'D1' falls off the left edge"});
}

TEST_CASE("placer: no grid parts leaves the bands alone", "[board]") {
    const Placement p = Placed("part J1 POWER\n", "edge J1 front middle\n");
    CHECK(p.columns == 0);
    CHECK(p.grid.empty());
    CHECK(p.outlineMin == Point{15 * kMm, 15 * kMm});
    CHECK(p.outlineMax.x - p.outlineMin.x == 10 * kMm);  // two bare margins
    CHECK(Inside(Courtyard(p, "J1"), p.outlineMin, p.outlineMax));
}

TEST_CASE("placer: chips joined by many Nets end side by side", "[board]") {
    constexpr std::string_view kChips = "part U1 74LS04\npart U2 74LS04\npart U3 74LS04\n"
                                        "part U4 74LS04\npart U5 74LS04\npart U6 74LS04\n"
                                        "net A U1.1Y U4.1A\nnet B U1.2Y U4.2A\n"
                                        "net C U1.3Y U4.3A\nnet D U1.4Y U4.4A\n"
                                        "net E U4.1Y U1.1A\nnet F U4.2Y U1.2A\n"
                                        "net G U4.3Y U1.3A\nnet H U4.4Y U1.4A\n"
                                        "net I U2.1Y U3.1A\nnet J U5.1Y U6.1A\n";
    for (const std::uint64_t seed : {1, 2, 3}) {
        const auto p = Place(kChips, "grid 4 4\n", {.seed = seed, .tries = 1});
        REQUIRE(p.has_value());
        const Point a = Part(*p, "U1").origin, b = Part(*p, "U4").origin;
        const std::int64_t dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
        INFO("seed " << seed);
        CHECK(((dx == 16 * kMm && dy == 0) || (dx == 0 && dy == 27 * kMm)));
    }
}

TEST_CASE("placer: the same options give the same placement", "[board]") {
    const auto a = Placed(kLeds, "edge J1 left middle\nrow front middle D1 D2 D3\n");
    const auto b = Placed(kLeds, "edge J1 left middle\nrow front middle D1 D2 D3\n");
    CHECK(a.grid == b.grid);
    CHECK(a.wireLength == b.wireLength);
    REQUIRE(a.parts.size() == b.parts.size());
    for (std::size_t i = 0; i < a.parts.size(); ++i) {
        CHECK(a.parts[i].origin == b.parts[i].origin);
    }
}

TEST_CASE("placer: tennis", "[board][slow]") {
    const auto circuit = LoadNetlistFile(std::string(BW_NETLIST_DIR) + "/tennis.bwn");
    REQUIRE(circuit.has_value());
    const auto rules = ParsePlaceRules("edge J1 left middle\n"
                                       "edge J2 back right\n"
                                       "edge J3 front left\n"
                                       "edge J4 front right\n"
                                       "edge SW1 front middle\n"
                                       "next R1 SW1 right\n",
                                       "tennis.place");
    REQUIRE(rules.has_value());
    const auto p = PlaceBoard(**circuit, *rules, "tennis.place", Footprints(), {.tries = 1});
    REQUIRE(p.has_value());
    CHECK(p->columns == 12);
    CHECK(p->rows == 5);
    for (std::size_t i = 0; i < p->parts.size(); ++i) {
        const Box a = Courtyard(*p, p->parts[i].reference);
        INFO(p->parts[i].reference);
        CHECK(Inside(a, p->outlineMin, p->outlineMax));
        for (std::size_t j = i + 1; j < p->parts.size(); ++j) {
            INFO(p->parts[j].reference);
            CHECK_FALSE(Overlap(a, Courtyard(*p, p->parts[j].reference)));
        }
    }
    const auto nearFront = [&](std::string_view r) {
        return Courtyard(*p, r).max.y > p->outlineMax.y - kMm - kSnap;
    };
    CHECK(nearFront("J3"));
    CHECK(nearFront("J4"));
    CHECK(nearFront("SW1"));
    CHECK(Courtyard(*p, "J3").max.x < Courtyard(*p, "SW1").min.x);
    CHECK(Courtyard(*p, "R1").min.x > Courtyard(*p, "SW1").max.x);
    CHECK(Courtyard(*p, "R1").max.x < Courtyard(*p, "J4").min.x);
    CHECK(Courtyard(*p, "J1").min.x < p->outlineMin.x + kMm + kSnap);
    CHECK(Courtyard(*p, "J2").min.y < p->outlineMin.y + kMm + kSnap);
    CHECK(Courtyard(*p, "J2").max.x > p->outlineMax.x - kMm - kSnap);
    CHECK(p->wireLength < 12'000 * kMm);
    WARN("tennis wire length " << static_cast<double>(p->wireLength) / 1e9 << " m");
}
