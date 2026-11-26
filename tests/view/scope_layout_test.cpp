#include "view/scope_layout.hpp"

#include <array>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

std::array<Box, 12> Strip(const ScopeCaseLayout& l) {
    return {l.timeLess,      l.timeValue,    l.timeMore, l.positionLess,
            l.positionValue, l.positionMore, l.autoHalf, l.normHalf,
            l.lamp,          l.logo,         l.sizeKey,  l.hasWords ? l.words : Box{}};
}

void CheckStrip(const ScopeCaseLayout& l, float width) {
    const auto boxes = Strip(l);
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        for (std::size_t j = i + 1; j < boxes.size(); ++j) {
            CHECK_FALSE(boxes[i].overlaps(boxes[j]));
        }
        CHECK(boxes[i].y1 <= kScopeStrip);
    }
    CHECK(l.sizeKey.x1 <= width - kScopeClose);  // clear of CaseClose's corner
    CHECK_FALSE(l.timeGroup.overlaps(l.triggerGroup));
    CHECK((l.hasWords ? l.words : l.logo).x1 <= l.timeGroup.x0);  // the badge first
    CHECK(l.triggerGroup.x1 <= l.sizeKey.x0);
}

}  // namespace

TEST_CASE("scope layout: the strip at the minimal size", "[view]") {
    const ScopeCaseLayout l = LayOutScopeCase(kScopeMinWidth, kScopeMinHeight);
    CheckStrip(l, kScopeMinWidth);
    CHECK_FALSE(l.hasWords);
    CHECK(l.timeLess.height() == 44);  // a finger's
    CHECK(l.screen.y0 >= kScopeStrip);
}

TEST_CASE("scope layout: the maker's words where there is room", "[view]") {
    CHECK_FALSE(LayOutScopeCase(660, 520).hasWords);
    const ScopeCaseLayout l = LayOutScopeCase(680, 520);  // the normal size
    CHECK(l.hasWords);
    CheckStrip(l, 680);
}

TEST_CASE("scope layout: the bands", "[view]") {
    const ScopeCaseLayout l = LayOutScopeCase(kScopeMinWidth, kScopeMinHeight);
    const std::vector<Box> four = ScopeBands(l.screen, 18, 4);
    REQUIRE(four.size() == 4);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(four[i].height() >= kMinBand);
        if (i > 0) {
            CHECK(four[i].y0 - four[i - 1].y1 == kBandGap);
        }
    }
    CHECK(four.back().y1 <= l.screen.y1 + 0.01f);
    CHECK(ScopeBands(l.screen, 18, 0).empty());
    CHECK(ScopeBands(l.screen, 18, 1)[0].height() == l.screen.height() - 18);
    CHECK(BandAt(four, four[0].y0 - 100) == 0);
    CHECK(BandAt(four, (four[2].y0 + four[2].y1) / 2) == 2);
    CHECK(BandAt(four, four.back().y1 + 100) == 3);
}
