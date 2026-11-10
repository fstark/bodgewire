#include "board/footprint.hpp"

#include <cmath>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/catalogue.hpp"

using namespace bw;

namespace {

std::int64_t Mm(double mm) { return std::llround(mm * 1'000'000); }

}  // namespace

TEST_CASE("footprint: every catalogue package loads", "[board]") {
    std::vector<std::string_view> packages;
    for (const PartInfo& part : AllParts()) {
        packages.push_back(part.package);
    }
    const auto library = LoadFootprints(BW_FOOTPRINT_DIR, packages);
    if (!library) {
        for (const Diagnostic& d : library.error()) {
            UNSCOPED_INFO(Format(d));
        }
    }
    REQUIRE(library.has_value());
    for (const std::string_view package : packages) {
        INFO(package);
        const auto it = library->find(package);
        REQUIRE(it != library->end());
        const Footprint& footprint = it->second;
        CHECK_FALSE(footprint.pads.empty());
        for (const FootprintPad& pad : footprint.pads) {
            CHECK(pad.position.x > footprint.courtyardMin.x);
            CHECK(pad.position.x < footprint.courtyardMax.x);
            CHECK(pad.position.y > footprint.courtyardMin.y);
            CHECK(pad.position.y < footprint.courtyardMax.y);
        }
    }
}

TEST_CASE("footprint: DIP-14 pads and courtyard", "[board]") {
    const std::vector<std::string_view> packages{"DIP-14"};
    const auto library = LoadFootprints(BW_FOOTPRINT_DIR, packages);
    REQUIRE(library.has_value());
    const Footprint& dip = library->at("DIP-14");
    CHECK(dip.id == "Package_DIP:DIP-14_W7.62mm");
    REQUIRE(dip.pads.size() == 14);
    CHECK(dip.pads[0].number == "1");
    CHECK(dip.pads[0].position == Point{0, 0});
    CHECK(dip.pads[7].number == "8");
    CHECK(dip.pads[7].position == Point{Mm(7.62), Mm(15.24)});
    CHECK(dip.courtyardMin == Point{Mm(-1.06), Mm(-1.53)});
    CHECK(dip.courtyardMax == Point{Mm(8.67), Mm(16.77)});
    CHECK(dip.text.starts_with("(footprint \"DIP-14_W7.62mm\""));
}

TEST_CASE("footprint: errors", "[board]") {
    const std::vector<std::string_view> unknown{"SOIC-8"};
    const auto none = LoadFootprints(BW_FOOTPRINT_DIR, unknown);
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error()[0].message == "no footprint for package 'SOIC-8'");

    const std::vector<std::string_view> dip{"DIP-14"};
    const auto missing = LoadFootprints("/nonexistent", dip);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error()[0].message == "cannot read footprint");

    std::vector<Diagnostic> errors;
    ReadFootprint("(footprint \"x\" (layer \"F.Cu\"))", "L:x", "x.kicad_mod", errors);
    REQUIRE(errors.size() == 2);
    CHECK(errors[0].message == "footprint has no pads");
    CHECK(errors[1].message == "footprint has no courtyard");
}
