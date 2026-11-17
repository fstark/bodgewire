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
    for (const BenchObject instrument :
         {BenchObject::Analyzer, BenchObject::Scope, BenchObject::Meter}) {
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

TEST_CASE("bench places: the TV top right, the keyboard under it, the instruments along the bottom",
          "[view]") {
    const Box tv = Placed(BenchObject::Tv, FirstSize(BenchObject::Tv, kBench));
    CHECK(tv.right == Catch::Approx(kBench.width - kBenchMargin));
    CHECK(tv.top == Catch::Approx(kBenchMargin));
    const Box keyboard =
        Placed(BenchObject::Keyboard, FirstSize(BenchObject::Keyboard, kBench, 0.36f));
    CHECK(keyboard.top > tv.bottom);
    CHECK(keyboard.right == Catch::Approx(tv.right));

    const Box meter = Placed(BenchObject::Meter, NormalSize(BenchObject::Meter));
    CHECK(meter.right == Catch::Approx(kBench.width - kBenchMargin));
    CHECK(meter.bottom == Catch::Approx(kBench.height - kBenchMargin));
    const Box scope = Placed(BenchObject::Scope, NormalSize(BenchObject::Scope));
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
