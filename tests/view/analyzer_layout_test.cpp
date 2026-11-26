#include "view/analyzer_layout.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

std::vector<Box> Strip(const AnalyzerCaseLayout& l) {
    std::vector<Box> boxes{l.badge.logo, l.armKey,    l.sampleLess, l.sampleValue, l.sampleMore,
                           l.viewLess,   l.viewValue, l.viewMore,   l.fitKey,      l.sizeKey};
    if (l.badge.hasWords) {
        boxes.push_back(l.badge.words);
    }
    return boxes;
}

void CheckStrip(const AnalyzerCaseLayout& l, float width) {
    const std::vector<Box> boxes = Strip(l);
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        for (std::size_t j = i + 1; j < boxes.size(); ++j) {
            CHECK_FALSE(boxes[i].overlaps(boxes[j]));
        }
        CHECK(boxes[i].y1 <= kCaseStrip);
    }
    CHECK(l.sizeKey.x1 <= width - kCaseClose);  // clear of CaseClose's corner
    CHECK(l.viewGroup.x1 <= l.sizeKey.x0);
    const std::vector<Box> groups{l.captureGroup, l.sampleGroup, l.viewGroup};
    for (std::size_t i = 0; i + 1 < groups.size(); ++i) {
        CHECK(groups[i].x1 < groups[i + 1].x0);
    }
    CHECK(l.armKey.height() == 44);  // a finger's
}

}  // namespace

TEST_CASE("analyzer layout: the strip at the minimal size", "[view]") {
    const AnalyzerCaseLayout l = LayOutAnalyzerCase(kAnalyzerMinWidth, kAnalyzerMinHeight);
    CheckStrip(l, kAnalyzerMinWidth);
    CHECK_FALSE(l.badge.hasWords);
    CHECK(l.screen.y0 >= kCaseStrip);
}

TEST_CASE("analyzer layout: the words from about 650 px", "[view]") {
    CHECK_FALSE(LayOutAnalyzerCase(640, 400).badge.hasWords);
    const AnalyzerCaseLayout at660 = LayOutAnalyzerCase(660, 400);
    CHECK(at660.badge.hasWords);
    CheckStrip(at660, 660);
}

TEST_CASE("analyzer layout: the words at the normal size", "[view]") {
    const AnalyzerCaseLayout l = LayOutAnalyzerCase(940, 520);
    CHECK(l.badge.hasWords);
    CheckStrip(l, 940);
}

TEST_CASE("analyzer layout: the screen's corner and rows", "[view]") {
    const AnalyzerCaseLayout l = LayOutAnalyzerCase(940, 520);
    for (const Box& key : {l.pinHalf, l.busHalf, l.groupKey}) {
        CHECK(key.height() >= 28);
        CHECK(key.x1 <= l.laneX0);
        CHECK(key.y1 <= l.rows.y0);
        CHECK(key.x0 >= l.screen.x0);
    }
    CHECK(l.count.x0 >= l.groupKey.x1);
    CHECK(l.count.x1 <= l.laneX0);
    CHECK(l.overview.x0 >= l.laneX0);
    CHECK(l.axis.x0 == l.laneX0);
    CHECK(l.overview.y1 <= l.axis.y0);
    CHECK(l.axis.y1 <= l.rows.y0);
    CHECK(l.rows.y1 == l.screen.y1);
    CHECK(l.laneX0 - l.screen.x0 == kAnalyzerPlate);
}

TEST_CASE("analyzer layout: row heights", "[view]") {
    CHECK(AnalyzerRowHeight(300, 1) == kRowMax);
    CHECK(AnalyzerRowHeight(300, 5) == kRowMax);   // capped: the screen goes on under them
    CHECK(AnalyzerRowHeight(300, 20) == kRowMin);  // scrolled
    CHECK(AnalyzerRowHeight(300, 8) == 300.0f / 8 - kRowGap);
    CHECK(AnalyzerRowHeight(300, 0) == kRowMax);
}
