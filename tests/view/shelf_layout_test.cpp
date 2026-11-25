#include "view/shelf_layout.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("shelf layout: trays from the left, the Machine's from the right", "[view]") {
    const ShelfMetrics m{.cell = 40, .cellGap = 4, .groupGap = 20};
    const std::vector<ShelfCell> items{
        {.tray = 0},                                            // Leave
        {.tray = 1},                                            // Power
        {.tray = 1, .cells = 2},                                // run switch
        {.tray = 2, .right = true},                             // binder
        {.tray = 2, .cells = 3, .right = true, .apart = true},  // Machine slot
    };
    const ShelfPlaces places = LayOutShelf(items, 1000, m);
    REQUIRE(places.x.size() == items.size());
    CHECK(places.x[0] == 10);            // half a tray gap from the edge
    CHECK(places.x[1] == 10 + 40 + 20);  // a tray's gap after the first
    CHECK(places.x[2] == places.x[1] + 40 + 4);
    CHECK(places.leftEnd == places.x[2] + 40 * 2 + 4);
    const float slot = 40 * 3 + 4 * 2;
    CHECK(places.x[4] + slot == 1000 - 10);  // the Machine slot ends half a gap from the edge
    CHECK(places.x[4] - (places.x[3] + 40) == 10);  // set apart: half a tray gap
    CHECK(places.rightStart == places.x[3]);
}

TEST_CASE("shelf layout: the narrow cells where the BW-2001's Shelf does not fit", "[view]") {
    // The BW-2001 on a Puzzle: Leave; Power, the run switch, Reset; five tools; the probe and
    // three Instruments; the brief, the binder, the Machine slot (a TV and a keyboard).
    std::vector<ShelfCell> items{{.tray = 0}, {.tray = 1}, {.tray = 1, .cells = 2}, {.tray = 1}};
    items.insert(items.end(), 5, {.tray = 2});
    items.push_back({.tray = 3, .cells = 2});
    items.insert(items.end(), 3, {.tray = 3});
    items.insert(items.end(), 2, {.tray = 4, .right = true});
    items.push_back({.tray = 4, .cells = 2, .right = true, .apart = true});
    CHECK(ShelfWidth(items, kShelfMetrics) == 1074);
    CHECK(ShelfWidth(items, kShelfNarrow) == 954);
    CHECK(ShelfMetricsFor(items, 1264).cell == kShelfMetrics.cell);  // a 1280 px window
    CHECK(ShelfMetricsFor(items, 1064).cell == kShelfNarrow.cell);   // a 1080 px one
    // At the width it needs, the two sides are a tray's gap apart.
    const ShelfPlaces places = LayOutShelf(items, 1074, kShelfMetrics);
    CHECK(places.rightStart - places.leftEnd == kShelfMetrics.groupGap);
}
