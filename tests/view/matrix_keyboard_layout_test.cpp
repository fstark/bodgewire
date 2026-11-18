#include "view/matrix_keyboard_layout.hpp"

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "parts/matrix_keyboard.hpp"
#include "parts/zx80_keys.hpp"

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

TEST_CASE("matrix keyboard layout: the ZX80's caps, every key once, inside, none overlapping",
          "[view]") {
    const auto caps = Zx80Caps();
    const auto keys = Zx80Keys();
    REQUIRE(caps.size() == keys.size());
    std::vector<int> seen(keys.size(), 0);
    for (const MatrixCap& cap : caps) {
        REQUIRE(cap.key < keys.size());
        ++seen[cap.key];
        CHECK(cap.x >= 0);
        CHECK(cap.x + cap.width <= kZx80KeyboardWidth);
        CHECK(cap.y >= 0);
        CHECK(cap.y + kZx80CapHeight <= kZx80KeyboardHeight);
    }
    for (std::size_t k = 0; k < keys.size(); ++k) {
        INFO(keys[k].legend);
        CHECK(seen[k] == 1);
    }
    for (std::size_t i = 0; i < caps.size(); ++i) {
        for (std::size_t j = i + 1; j < caps.size(); ++j) {
            const bool apart = caps[i].y + 1 <= caps[j].y || caps[j].y + 1 <= caps[i].y ||
                               caps[i].x + caps[i].width <= caps[j].x ||
                               caps[j].x + caps[j].width <= caps[i].x;
            INFO(keys[caps[i].key].legend << " and " << keys[caps[j].key].legend);
            CHECK(apart);
        }
    }
    // The rows as printed: 1 at the top left, SHIFT at the bottom left, P at the right.
    const auto at = [&](std::string_view legend) {
        for (const MatrixCap& cap : caps) {
            if (keys[cap.key].legend == legend) {
                return cap;
            }
        }
        FAIL("no cap " << legend);
        return caps[0];
    };
    CHECK(at("1").y < at("Q").y);
    CHECK(at("Q").y < at("A").y);
    CHECK(at("A").y < at("SHIFT").y);
    CHECK(at("P").x > at("O").x);
    CHECK(at("0").x > at("9").x);
}
