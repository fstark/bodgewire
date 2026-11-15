#include "view/matrix_keyboard_layout.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/matrix_keyboard.hpp"

using namespace bw;

TEST_CASE("matrix keyboard layout: every key once, inside the keyboard, none overlapping",
          "[view]") {
    const auto caps = Bw2001Caps();
    const auto keys = Bw2001Keys();
    REQUIRE(caps.size() == keys.size());
    std::vector<int> seen(keys.size(), 0);
    for (const MatrixCap& cap : caps) {
        REQUIRE(cap.key < keys.size());
        ++seen[cap.key];
        CHECK(cap.x >= 0);
        CHECK(cap.x + cap.width <= kMatrixKeyboardWidth);
        CHECK(cap.y >= 0);
        CHECK(cap.y < kMatrixKeyboardRows);
    }
    for (std::size_t k = 0; k < keys.size(); ++k) {
        INFO(keys[k].legend);
        CHECK(seen[k] == 1);
    }
    for (std::size_t i = 0; i < caps.size(); ++i) {
        for (std::size_t j = i + 1; j < caps.size(); ++j) {
            const bool apart = caps[i].y != caps[j].y || caps[i].x + caps[i].width <= caps[j].x ||
                               caps[j].x + caps[j].width <= caps[i].x;
            INFO(keys[caps[i].key].legend << " and " << keys[caps[j].key].legend);
            CHECK(apart);
        }
    }
}
