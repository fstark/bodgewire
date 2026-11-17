#include "view/tv_controls.hpp"

#include <algorithm>
#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace bw;
using Catch::Approx;

TEST_CASE("tv controls: at the marks the grey is the picture as before", "[view]") {
    const TvControls controls;
    CHECK(controls.tone() == TvTone{});
    for (int i = -256; i <= 768; ++i) {
        const float cell = static_cast<float>(i) / 512;
        INFO("cell " << cell);
        CHECK(Grey(cell, controls.tone()) == std::lround(std::clamp(cell, 0.0f, 1.0f) * 255.0f));
    }
}

TEST_CASE("tv controls: turns clamped, a reset back at the mark", "[view]") {
    TvControls controls;
    for (const TvKnob knob : kTvKnobs) {
        CHECK(controls.turn(knob) == Mark(knob));
        controls.set(knob, 1.7);
        CHECK(controls.turn(knob) == 1.0);
        controls.set(knob, -0.2);
        CHECK(controls.turn(knob) == 0.0);
        controls.reset(knob);
        CHECK(controls.turn(knob) == Mark(knob));
    }
    CHECK(controls.on());
    CHECK_FALSE(controls.underscan());
}

TEST_CASE("tv controls: the shift, clockwise right and up", "[view]") {
    TvControls controls;
    CHECK(controls.shift().x == 0.0f);
    CHECK(controls.shift().y == 0.0f);
    controls.set(TvKnob::HPos, 1);
    controls.set(TvKnob::VPos, 1);
    CHECK(controls.shift().x == Approx(0.25));
    CHECK(controls.shift().y == Approx(-0.25));  // up: screen y grows down
    controls.set(TvKnob::HPos, 0);
    controls.set(TvKnob::VPos, 0);
    CHECK(controls.shift().x == Approx(-0.25));
    CHECK(controls.shift().y == Approx(0.25));
}

TEST_CASE("tv controls: the tone's ends", "[view]") {
    TvControls controls;
    controls.set(TvKnob::Contrast, 0);
    CHECK(Grey(1.0f, controls.tone()) == 0);  // no gain, black at the mark
    controls.set(TvKnob::Contrast, 1);
    CHECK(controls.tone().gain == Approx(4.0 / 3));
    CHECK(Grey(0.75f, controls.tone()) == 255);
    controls.set(TvKnob::Bright, 1);
    CHECK(controls.tone().black == Approx(0.3));
    CHECK(Grey(0.0f, controls.tone()) == std::lround(0.3f * 255));
    controls.set(TvKnob::Bright, 0);
    CHECK(controls.tone().black == Approx(-0.1));
    CHECK(Grey(0.05f, controls.tone()) == 0);
}

TEST_CASE("tv controls: the window, a real monitor's or the whole frame", "[view]") {
    TvControls controls;
    // A frame of N line starts paints rows 0..N: the window's bottom is N + 1.
    CHECK(controls.window(260) == PictureWindow{80, 496, 20, 261});
    CHECK(controls.window(312) == PictureWindow{80, 496, 20, 313});
    CHECK(controls.window(319) == PictureWindow{80, 496, 20, 320});
    CHECK(controls.window(320) == PictureWindow{80, 496, 20, 320});
    CHECK(controls.window(0) == PictureWindow{80, 496, 20, 320});    // no frame yet
    CHECK(controls.window(330) == PictureWindow{80, 496, 20, 320});  // past the frame's rows
    CHECK(controls.window(119) == PictureWindow{80, 496, 20, 320});  // no sensible frame
    CHECK(controls.window(120) == PictureWindow{80, 496, 20, 121});
    controls.setUnderscan(true);
    CHECK(controls.window(260) == PictureWindow{0, 512, 0, 320});
}

TEST_CASE("tv controls: --tv parsing", "[view]") {
    const auto parsed = ParseTvSettings("hpos:31%,vpos:60.5%,bright:0%,contrast:100%");
    REQUIRE(parsed);
    CHECK(parsed->turn(TvKnob::HPos) == Approx(0.31));
    CHECK(parsed->turn(TvKnob::VPos) == Approx(0.605));
    CHECK(parsed->turn(TvKnob::Bright) == 0.0);
    CHECK(parsed->turn(TvKnob::Contrast) == 1.0);
    CHECK(parsed->on());

    const auto switches = ParseTvSettings("off,underscan");
    REQUIRE(switches);
    CHECK_FALSE(switches->on());
    CHECK(switches->underscan());
    CHECK(switches->turn(TvKnob::HPos) == 0.5);

    TvControls start;
    start.set(TvKnob::Bright, 0.9);
    const auto over = ParseTvSettings("hpos:10%", start);
    REQUIRE(over);
    CHECK(over->turn(TvKnob::Bright) == 0.9);

    for (const char* bad : {"", "hpos", "hpos:31", "hpos:101%", "size:50%", "hpos:31%,", "hpos:%",
                            "hpos:3.%", "on", "hpos:-1%", "hpos:1e1%", "hpos:inf%"}) {
        INFO(bad);
        CHECK_FALSE(ParseTvSettings(bad));
    }
}
