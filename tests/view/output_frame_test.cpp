#include "view/output_frame.hpp"

#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "board/kicad_board.hpp"
#include "session/session.hpp"

using namespace bw;

namespace {

// The cheap fixture (board_scene_test's): the design netlist and its Board, no LoadMachine.
struct Fixture {
    std::unique_ptr<LoadedCircuit> design;
    std::unique_ptr<BoardScene> scene;
};

Fixture Load(const std::string& name) {
    auto design = LoadNetlistFile(test::NetlistPath(name));
    REQUIRE(design.has_value());
    auto board = ReadKicadBoardFile(test::BoardPath(name));
    REQUIRE(board.has_value());
    auto scene = std::make_unique<BoardScene>(*board, *board, **design);
    return {std::move(*design), std::move(scene)};
}

bool Same(Bounds a, Bounds b) {
    return a.min.x == b.min.x && a.min.y == b.min.y && a.max.x == b.max.x && a.max.y == b.max.y;
}

double Area(Bounds b) {
    return static_cast<double>(b.max.x - b.min.x) * static_cast<double>(b.max.y - b.min.y);
}

}  // namespace

TEST_CASE("output frame: small Boards whole", "[view]") {
    for (const char* name : {"counter", "chaser", "dice", "knob"}) {
        INFO(name);
        const Fixture f = Load(name);
        CHECK(Same(OutputFrame(*f.scene, false), f.scene->bounds()));
    }
}

TEST_CASE("output frame: framed on the lamps", "[view]") {
    for (const char* name : {"stopwatch", "led_computer", "bw1"}) {
        INFO(name);
        const Fixture f = Load(name);
        const Bounds board = f.scene->bounds();
        const Bounds frame = OutputFrame(*f.scene, false);
        CHECK(Area(frame) < kWholeShare * Area(board));
        CHECK(frame.min.x >= board.min.x);
        CHECK(frame.min.y >= board.min.y);
        CHECK(frame.max.x <= board.max.x);
        CHECK(frame.max.y <= board.max.y);
        int lamps = 0;
        for (std::size_t k = 0; k < f.scene->board().components.size(); ++k) {
            if (!f.scene->lamp(k)) {
                continue;
            }
            ++lamps;
            for (const Point p : f.scene->courtyard(k)) {
                CHECK(p.x >= frame.min.x);
                CHECK(p.x <= frame.max.x);
                CHECK(p.y >= frame.min.y);
                CHECK(p.y <= frame.max.y);
            }
        }
        CHECK(lamps > 0);
        if (std::string(name) == "stopwatch") {
            CHECK(frame.min.y == board.min.y);  // the digits along the top: clamped
        }
        if (std::string(name) == "bw1") {
            CHECK(frame.max.y < board.max.y);
        }
    }
}

TEST_CASE("output frame: a TV, whole", "[view]") {
    const Fixture f = Load("stopwatch");
    CHECK(Same(OutputFrame(*f.scene, true), f.scene->bounds()));
}

TEST_CASE("output frame: HasTv", "[view]") {
    for (const char* name :
         {"test_pattern", "ball", "tennis", "message", "typewriter", "bw2001", "bw80"}) {
        INFO(name);
        const auto design = LoadNetlistFile(test::NetlistPath(name));
        REQUIRE(design.has_value());
        CHECK(HasTv(**design));
    }
    for (const char* name :
         {"counter", "chaser", "dice", "stopwatch", "knob", "led_computer", "bw1"}) {
        INFO(name);
        const auto design = LoadNetlistFile(test::NetlistPath(name));
        REQUIRE(design.has_value());
        CHECK_FALSE(HasTv(**design));
    }
}
