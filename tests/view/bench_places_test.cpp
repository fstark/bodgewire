#include "view/bench_places.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

// 1280 × 800: the main window's content above the shelf (padding 8, the shelf 64, spacing 4).
constexpr Extent kBench{1264, 716};

struct Box {
    float left, top, right, bottom;
};

Box Placed(BenchObject object, Extent size) {
    const Place place = DefaultPlace(object, kBench);
    const float left = place.x - place.pivotX * size.width;
    const float top = place.y - place.pivotY * size.height;
    return {left, top, left + size.width, top + size.height};
}

bool Inside(const Box& box) {
    return box.left >= 0 && box.top >= 0 && box.right <= kBench.width &&
           box.bottom <= kBench.height;
}

}  // namespace

TEST_CASE("bench places: every object at its first size lies inside the bench", "[view]") {
    for (const BenchObject instrument : {BenchObject::Analyzer, BenchObject::Scope}) {
        CHECK(Inside(Placed(instrument, NormalSize(instrument))));
        CHECK(Inside(Placed(instrument, MinimalSize(instrument))));
    }
    CHECK(Inside(Placed(BenchObject::Tv, FirstSize(BenchObject::Tv, kBench))));
    CHECK(Inside(Placed(BenchObject::Knob, FirstSize(BenchObject::Knob, kBench, 1, 2))));
    // The BW-2001's chiclets, the typewriter's keys, the ZX80's membrane.
    for (const float aspect : {0.36f, 0.43f, 0.54f}) {
        CHECK(Inside(
            Placed(BenchObject::Keyboard, FirstSize(BenchObject::Keyboard, kBench, aspect))));
    }
    for (const BenchObject window : {BenchObject::Manual, BenchObject::Brief, BenchObject::Debug}) {
        CHECK(Inside(Placed(window, FirstSize(window, kBench))));
    }
}

TEST_CASE(
    "bench places: the TV top right, the keyboard under it, the instruments along the bottom, "
    "the view buttons' lane left free",
    "[view]") {
    const Box tv = Placed(BenchObject::Tv, FirstSize(BenchObject::Tv, kBench));
    CHECK(tv.right == Catch::Approx(kBench.width - kBenchMargin));
    CHECK(tv.top == Catch::Approx(kBenchMargin));
    const Box keyboard =
        Placed(BenchObject::Keyboard, FirstSize(BenchObject::Keyboard, kBench, 0.36f));
    CHECK(keyboard.top > tv.bottom);
    CHECK(keyboard.right == Catch::Approx(tv.right - kViewLane));

    const Box scope = Placed(BenchObject::Scope, MinimalSize(BenchObject::Scope));
    CHECK((scope.left + scope.right) / 2 == Catch::Approx(kBench.width / 2));
    const Box analyzer = Placed(BenchObject::Analyzer, NormalSize(BenchObject::Analyzer));
    CHECK(analyzer.left == Catch::Approx(kBenchMargin));
    CHECK(analyzer.bottom == Catch::Approx(kBench.height - kBenchMargin));

    // The brief at the top centre clear of the TV.
    const Box brief = Placed(BenchObject::Brief, FirstSize(BenchObject::Brief, kBench));
    CHECK((brief.left + brief.right) / 2 == Catch::Approx(kBench.width / 2));
    CHECK(brief.right < tv.left);

    const Box knob = Placed(BenchObject::Knob, FirstSize(BenchObject::Knob, kBench, 1, 1));
    CHECK(knob.left == Catch::Approx(kBenchMargin));
    CHECK(knob.top == Catch::Approx(kBenchMargin));
}

TEST_CASE("bench places: a keyboard keeps its first width when it fits, narrows when not",
          "[view]") {
    CHECK(FirstSize(BenchObject::Keyboard, kBench, 0.36f).width ==
          Catch::Approx(kKeyboardFirstWidth));
    const Extent membrane = FirstSize(BenchObject::Keyboard, kBench, 0.54f);
    CHECK(membrane.width < kKeyboardFirstWidth);
    CHECK(membrane.height == Catch::Approx(membrane.width * 0.54f));
}

TEST_CASE("bench places: the size button goes to the preset further away", "[view]") {
    const Extent minimal{240, 150}, normal{320, 200};
    const auto same = [](Extent a, Extent b) { return a.width == b.width && a.height == b.height; };
    CHECK(same(NextPreset(normal, minimal, normal), minimal));
    CHECK(same(NextPreset(minimal, minimal, normal), normal));
    CHECK(same(NextPreset({310, 190}, minimal, normal), minimal));  // near normal
    CHECK(same(NextPreset({250, 150}, minimal, normal), normal));   // near minimal
    CHECK(same(NextPreset({600, 400}, minimal, normal), minimal));  // larger than both
}

TEST_CASE("bench places: on the bench the size button goes round minimal, normal, full", "[view]") {
    const Extent minimal{240, 150}, normal{320, 220};
    const auto same = [](Extent a, Extent b) { return a.width == b.width && a.height == b.height; };
    CHECK(same(NextPreset(minimal, minimal, normal, kBench), normal));
    CHECK(same(NextPreset(normal, minimal, normal, kBench), kBench));
    CHECK(same(NextPreset(kBench, minimal, normal, kBench), minimal));
    // From a size between two: as from the nearer one.
    CHECK(same(NextPreset({300, 200}, minimal, normal, kBench), kBench));
    CHECK(same(NextPreset({900, 600}, minimal, normal, kBench), minimal));
}

TEST_CASE("bench places: an object fills the bench, one that keeps its proportions inside it",
          "[view]") {
    const Extent free = FillBench(kBench);
    CHECK(free.width == Catch::Approx(kBench.width));
    CHECK(free.height == Catch::Approx(kBench.height));
    const Extent tv = FillBench(kBench, kTvAspect);  // by the height
    CHECK(tv.height == Catch::Approx(kBench.height));
    CHECK(tv.width == Catch::Approx(kBench.height / kTvAspect));
    const Extent keyboard = FillBench(kBench, 0.36f);  // by the width
    CHECK(keyboard.width == Catch::Approx(kBench.width));
    CHECK(keyboard.height == Catch::Approx(kBench.width * 0.36f));
}

TEST_CASE("bench places: a popped-out object goes right of the main window, its top kept",
          "[view]") {
    const Area display{0, 25, 2560, 1390};
    const Area main{200, 100, 1280, 800};
    const auto place = BesidePlace({300, 400, 600, 500}, main, std::span(&display, 1));
    REQUIRE(place);
    CHECK(place->x == Catch::Approx(200 + 1280 + kBenchMargin));
    CHECK(place->y == Catch::Approx(400));
    CHECK(place->width == Catch::Approx(600));
    CHECK(place->height == Catch::Approx(500));
}

TEST_CASE("bench places: a popped-out object moves up to fit the display's bottom", "[view]") {
    const Area display{0, 25, 2560, 1390};
    const auto place =
        BesidePlace({300, 1100, 600, 500}, {200, 100, 1280, 800}, std::span(&display, 1));
    REQUIRE(place);
    CHECK(place->y == Catch::Approx(25 + 1390 - 500));
}

TEST_CASE("bench places: left of the main window when the right has no room", "[view]") {
    const Area display{0, 25, 2560, 1390};
    const auto place =
        BesidePlace({1500, 200, 600, 500}, {1000, 100, 1280, 800}, std::span(&display, 1));
    REQUIRE(place);
    CHECK(place->x == Catch::Approx(1000 - kBenchMargin - 600));
    CHECK(place->y == Catch::Approx(200));
}

TEST_CASE("bench places: no room either side, the object stays where it was", "[view]") {
    const Area display{0, 25, 1512, 957};
    CHECK_FALSE(BesidePlace({300, 200, 600, 500}, {100, 60, 1280, 800}, std::span(&display, 1)));
    // Too tall for the display beside it.
    const Area wide{0, 0, 3000, 400};
    CHECK_FALSE(BesidePlace({300, 0, 600, 500}, {100, 0, 1280, 400}, std::span(&wide, 1)));
}

TEST_CASE("bench places: a popped-out object goes on a second display right of the first",
          "[view]") {
    const Area displays[] = {{0, 25, 1512, 957}, {1512, 0, 1920, 1080}};
    const auto place = BesidePlace({300, 200, 600, 500}, {100, 60, 1400, 800}, displays);
    REQUIRE(place);
    CHECK(place->x == Catch::Approx(100 + 1400 + kBenchMargin));
    CHECK(place->y == Catch::Approx(200));
}

TEST_CASE("bench places: back on the bench, a size kept or shrunk in proportion", "[view]") {
    const Extent fits = FitBench({600, 500}, kBench);
    CHECK(fits.width == Catch::Approx(600));
    CHECK(fits.height == Catch::Approx(500));
    const Extent wide = FitBench({2000, 1000}, kBench);  // by the width
    CHECK(wide.width == Catch::Approx(1264));
    CHECK(wide.height == Catch::Approx(632));
    const Extent tall = FitBench({800, 1432}, kBench);  // by the height
    CHECK(tall.width == Catch::Approx(400));
    CHECK(tall.height == Catch::Approx(716));
}

TEST_CASE("bench places: on a display further left, at its edge nearest the main window",
          "[view]") {
    // The laptop's display, and one up and to the left whose right edge is the laptop's left.
    const Area displays[] = {{0, 33, 1512, 949}, {-3008, -1114, 3008, 1662}};
    const auto place = BesidePlace({140, 331, 600, 500}, {116, 107, 1280, 800}, displays);
    REQUIRE(place);
    CHECK(place->x == Catch::Approx(-600));
    CHECK(place->y == Catch::Approx(-1114 + 1662 - 500));
}

TEST_CASE("bench places: popped back in, where it was, moved inside the bench", "[view]") {
    const Place inside = ReturnPlace({300, 200}, {400, 300}, kBench);
    CHECK(inside.x == Catch::Approx(300));
    CHECK(inside.y == Catch::Approx(200));
    // Past the right and bottom edges (the bench shrank since): moved in to the margin.
    const Place past = ReturnPlace({1200, 700}, {400, 300}, kBench);
    CHECK(past.x == Catch::Approx(kBench.width - kBenchMargin - 400));
    CHECK(past.y == Catch::Approx(kBench.height - kBenchMargin - 300));
    const Place before = ReturnPlace({-50, 2}, {400, 300}, kBench);
    CHECK(before.x == Catch::Approx(kBenchMargin));
    CHECK(before.y == Catch::Approx(kBenchMargin));
    // As wide as the bench: centred on that side.
    const Place wide = ReturnPlace({500, 100}, {kBench.width, 300}, kBench);
    CHECK(wide.x == Catch::Approx(0));
    CHECK(wide.y == Catch::Approx(100));
}
