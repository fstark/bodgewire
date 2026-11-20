#include "view/chip_body.hpp"

#include <memory>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "../paths.hpp"
#include "board/kicad_board.hpp"
#include "netlist/builder.hpp"

using namespace bw;

TEST_CASE("chip body: which Parts have a memory window", "[view]") {
    CHECK(MemoryLookOf("2716") == MemoryLook::Eprom);
    for (const char* part : {"2332", "2316B", "2513", "6530"}) {
        CHECK(MemoryLookOf(part) == MemoryLook::Mask);
    }
    CHECK(MemoryLookOf("74LS00") == MemoryLook::None);
    CHECK(MemoryLookOf("2114") == MemoryLook::None);  // RAM: no contents
}

TEST_CASE("chip body: the window on the LED Computer's 2716 and its hit", "[view]") {
    auto design = LoadNetlistFile(test::NetlistPath("led_computer"));
    REQUIRE(design.has_value());
    auto board = ReadKicadBoardFile(test::BoardPath("led_computer"));
    REQUIRE(board.has_value());
    const BoardScene scene(*board, *board, **design);
    const auto u2 = scene.componentIndex("U2");
    REQUIRE(u2.has_value());
    const BoardComponent& chip = scene.board().components[*u2];
    const auto body = BodyOf(chip);
    const auto window = MemoryWindowOf(chip, "2716");
    REQUIRE(body.has_value());
    REQUIRE(window.has_value());
    CHECK(window->centre.x == body->box.centre().x);
    CHECK(window->centre.y == body->box.centre().y);
    const std::int64_t across = body->horizontal ? body->box.height() : body->box.width();
    CHECK(window->quartz == across * 3 / 10);
    CHECK(window->seal == window->quartz * 23 / 20);

    const Point c = window->centre;
    const Point edge =
        body->horizontal ? Point{c.x, c.y + window->seal} : Point{c.x + window->seal, c.y};
    const Point outside = body->horizontal ? Point{c.x, c.y + window->seal + 100'000}
                                           : Point{c.x + window->seal + 100'000, c.y};
    CHECK(MemoryWindowAt(scene, c) == u2);
    CHECK(MemoryWindowAt(scene, edge) == u2);
    CHECK_FALSE(MemoryWindowAt(scene, outside).has_value());
    for (const Pad& pad : chip.pads) {  // the window never covers a pin
        CHECK_FALSE(MemoryWindowAt(scene, pad.position).has_value());
    }
    const auto u5 = scene.componentIndex("U5");  // a 74LS138
    REQUIRE(u5.has_value());
    CHECK_FALSE(MemoryWindowOf(scene.board().components[*u5], "74LS138").has_value());
    CHECK_FALSE(
        MemoryWindowAt(scene, BodyOf(scene.board().components[*u5])->box.centre()).has_value());
}
