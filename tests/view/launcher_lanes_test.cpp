#include "view/launcher_lanes.hpp"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

LauncherLanes Lanes() {
    LauncherLanes lanes;
    lanes.setPuzzles({3, 0, 2});
    return lanes;
}

}  // namespace

TEST_CASE("launcher lanes: start", "[view]") {
    const LauncherLanes lanes = Lanes();
    CHECK(lanes.lane() == Lane::Machines);
    CHECK(lanes.machine() == 0);
    CHECK(lanes.postIt() == 0);
    CHECK_FALSE(lanes.puzzle());
}

TEST_CASE("launcher lanes: left and right", "[view]") {
    LauncherLanes lanes = Lanes();
    lanes.left();  // the first Machine: stops
    CHECK(lanes.machine() == 0);
    lanes.right();
    lanes.right();
    lanes.right();  // the last: stops
    CHECK(lanes.machine() == 2);
    lanes.left();
    CHECK(lanes.machine() == 1);
    lanes.down();  // no Puzzles: post-it 0 only
    lanes.right();
    CHECK(lanes.postIt() == 0);
    lanes.up();
    lanes.left();
    lanes.down();
    lanes.right();
    lanes.right();
    lanes.right();
    lanes.right();  // 3 Puzzles: post-its 0..3
    CHECK(lanes.postIt() == 3);
    CHECK(lanes.puzzle() == 2);
    CHECK(lanes.machine() == 0);  // left and right in the bottom lane keep the Machine
    lanes.left();
    CHECK(lanes.postIt() == 2);
}

TEST_CASE("launcher lanes: another Machine starts the Machine", "[view]") {
    LauncherLanes lanes = Lanes();
    lanes.down();
    lanes.right();
    CHECK(lanes.postIt() == 1);
    lanes.up();
    CHECK(lanes.postIt() == 0);
    lanes.right();
    CHECK(lanes.machine() == 1);
    CHECK(lanes.postIt() == 0);
    lanes.selectPostIt(0);
    lanes.selectMachine(2);
    CHECK(lanes.lane() == Lane::Machines);
    CHECK(lanes.postIt() == 0);
}

TEST_CASE("launcher lanes: down and up", "[view]") {
    LauncherLanes lanes = Lanes();
    lanes.down();
    CHECK(lanes.lane() == Lane::PostIts);
    CHECK(lanes.postIt() == 0);  // kept
    lanes.right();
    lanes.right();
    lanes.up();
    CHECK(lanes.lane() == Lane::Machines);
    CHECK(lanes.postIt() == 0);
    lanes.down();
    CHECK(lanes.postIt() == 0);
}

TEST_CASE("launcher lanes: select", "[view]") {
    LauncherLanes lanes = Lanes();
    lanes.selectMachine(9);
    CHECK(lanes.machine() == 2);
    lanes.selectPostIt(9);
    CHECK(lanes.lane() == Lane::PostIts);
    CHECK(lanes.postIt() == 2);
    lanes.setPuzzles({1});  // fewer Machines: clamped
    CHECK(lanes.machine() == 0);
    CHECK(lanes.postIt() == 1);
    lanes.setPuzzles({});
    CHECK(lanes.machine() == 0);
    CHECK(lanes.postIt() == 0);
}

TEST_CASE("launcher lanes: glide", "[view]") {
    Glide glide;
    glide.jump(2);
    CHECK(glide.value == 2);
    CHECK_FALSE(glide.moving());
    glide.target = 3;
    CHECK(glide.moving());
    for (int frame = 0; frame < 15; ++frame) {  // 0.25 s at 60 fps
        glide.update(1.0 / 60);
        CHECK(glide.value <= 3);
    }
    CHECK(glide.value > 2.97);
    for (int frame = 0; frame < 45; ++frame) {
        glide.update(1.0 / 60);
    }
    CHECK(glide.value == 3);
    CHECK_FALSE(glide.moving());
}

TEST_CASE("launcher lanes: picture left", "[view]") {
    constexpr double w = 0.3;
    CHECK(PictureLeft(0, w) == Catch::Approx((kPictureFrom + kPictureTo - w) / 2));
    CHECK(PictureLeft(-1, w) + w == Catch::Approx(kHintLeft));
    CHECK(PictureLeft(1, w) == Catch::Approx(kHintRight));
    const double half = PictureLeft(0.5, w);
    CHECK(half > PictureLeft(0, w));
    CHECK(half < PictureLeft(1, w));
    CHECK(PictureLeft(1e-9, w) == Catch::Approx(PictureLeft(-1e-9, w)));
    CHECK(PictureLeft(2, w) > 1);       // past the lane's right edge
    CHECK(PictureLeft(-2, w) + w < 0);  // past its left edge
}

TEST_CASE("launcher lanes: a Random fault is the last post-it", "[view]") {
    LauncherLanes lanes;
    lanes.setPuzzles({2, 0, 1}, {true, true, false});
    // Machine 0: the card, two Puzzles, the Random fault.
    lanes.down();
    CHECK_FALSE(lanes.random());  // the card
    lanes.right();
    lanes.right();
    CHECK(lanes.puzzle() == 1u);
    CHECK_FALSE(lanes.random());
    lanes.right();
    CHECK(lanes.postIt() == 3);
    CHECK(lanes.random());
    CHECK_FALSE(lanes.puzzle().has_value());  // not a Puzzle of the list
    lanes.right();                            // the end stops
    CHECK(lanes.postIt() == 3);
    lanes.left();
    CHECK(lanes.puzzle() == 1u);
    lanes.selectPostIt(9);  // clamped onto the last: the Random fault
    CHECK(lanes.random());
    // A Machine with no Puzzle: the card, then the Random fault.
    lanes.selectMachine(1);
    lanes.selectPostIt(1);
    CHECK(lanes.postIt() == 1);
    CHECK(lanes.random());
    lanes.up();  // the top lane has post-it 0
    CHECK_FALSE(lanes.random());
}

TEST_CASE("launcher lanes: none without the file", "[view]") {
    LauncherLanes lanes;
    lanes.setPuzzles({2, 0, 1}, {true, true, false});
    lanes.selectMachine(2);  // one Puzzle, no Random fault
    lanes.selectPostIt(9);
    CHECK(lanes.postIt() == 1);
    CHECK(lanes.puzzle() == 0u);
    CHECK_FALSE(lanes.random());
    LauncherLanes plain;  // no `random` given: none has one
    plain.setPuzzles({2});
    plain.selectPostIt(9);
    CHECK(plain.postIt() == 2);
    CHECK_FALSE(plain.random());
}

TEST_CASE("launcher lanes: the selection kept across setPuzzles", "[view]") {
    // Back from a Random fault's bench: the list is read anew, the post-it still selected.
    LauncherLanes lanes;
    lanes.setPuzzles({2, 3}, {true, true});
    lanes.selectMachine(1);
    lanes.selectPostIt(4);
    REQUIRE(lanes.random());
    lanes.setPuzzles({2, 3}, {true, true});
    CHECK(lanes.machine() == 1);
    CHECK(lanes.postIt() == 4);
    CHECK(lanes.random());
    lanes.setPuzzles({2, 3}, {true, false});  // its file gone: clamped onto the last Puzzle
    CHECK(lanes.postIt() == 3);
    CHECK_FALSE(lanes.random());
}
