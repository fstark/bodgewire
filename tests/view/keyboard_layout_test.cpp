#include "view/keyboard_layout.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

const KeyCap& CapLabelled(std::string_view label) {
    const auto caps = KeyCaps();
    const auto it = std::ranges::find(caps, label, &KeyCap::label);
    REQUIRE(it != caps.end());
    return *it;
}

}  // namespace

TEST_CASE("keyboard layout: each code 0x20-0x5F on exactly one cap", "[view]") {
    for (unsigned code = 0x20; code <= 0x5F; ++code) {
        int caps = 0;
        for (const KeyCap& cap : KeyCaps()) {
            const auto plain = CapCode(cap, false, false);
            const auto shifted = CapCode(cap, true, false);
            caps += (plain == code || shifted == code) ? 1 : 0;
        }
        INFO("code " << code);
        CHECK(caps == 1);
    }
}

TEST_CASE("keyboard layout: shift is bit-paired", "[view]") {
    CHECK(CapCode(CapLabelled("1"), true, false) == '!');
    CHECK(CapCode(CapLabelled("2"), true, false) == '"');
    CHECK(CapCode(CapLabelled(":"), true, false) == '*');
    CHECK(CapCode(CapLabelled(";"), true, false) == '+');
    CHECK(CapCode(CapLabelled("-"), true, false) == '=');
    CHECK(CapCode(CapLabelled("/"), true, false) == '?');
    CHECK(CapCode(CapLabelled("K"), true, false) == '[');
    CHECK(CapCode(CapLabelled("P"), true, false) == '@');
    CHECK(CapCode(CapLabelled("O"), true, false) == '_');
    CHECK(CapCode(CapLabelled("A"), true, false) == 'A');
    CHECK(CapCode(CapLabelled("0"), false, false) == '0');
    CHECK_FALSE(CapCode(CapLabelled("0"), true, false));
    for (const KeyCap& cap : KeyCaps()) {
        if (!cap.shifted.empty()) {
            CHECK(CapCode(cap, true, false) == (cap.code ^ 0x10));
            CHECK(static_cast<std::uint8_t>(cap.shifted[0]) == (cap.code ^ 0x10));
        }
    }
}

TEST_CASE("keyboard layout: special keys and CTRL", "[view]") {
    CHECK(CapCode(CapLabelled("RETURN"), false, false) == 0x0D);
    CHECK(CapCode(CapLabelled("BACK SPACE"), true, false) == 0x08);
    CHECK(CapCode(CapLabelled("ESC"), false, true) == 0x1B);
    CHECK(CapCode(CapLabelled("A"), false, true) == 0x01);
    CHECK(CapCode(CapLabelled("G"), false, true) == 0x07);
    CHECK(CapCode(CapLabelled("P"), true, true) == 0x00);
    CHECK(CapCode(CapLabelled("K"), true, true) == 0x1B);
    CHECK(CapCode(CapLabelled("1"), false, true) == '1');
    CHECK_FALSE(CapCode(CapLabelled("SHIFT"), false, false));
    CHECK_FALSE(CapCode(CapLabelled("CTRL"), false, false));

    std::set<unsigned> reached;
    for (const KeyCap& cap : KeyCaps()) {
        for (const bool shift : {false, true}) {
            for (const bool ctrl : {false, true}) {
                if (const auto code = CapCode(cap, shift, ctrl)) {
                    reached.insert(*code);
                }
            }
        }
    }
    CHECK(reached.size() == 0x60);
    CHECK(*reached.rbegin() == 0x5F);
}

TEST_CASE("keyboard layout: CapFor", "[view]") {
    const auto caps = KeyCaps();
    for (unsigned code = 0; code <= 0x5F; ++code) {
        const auto press = CapFor(static_cast<std::uint8_t>(code));
        INFO("code " << code);
        REQUIRE(press);
        CHECK(CapCode(caps[press->cap], press->shift, press->ctrl) == code);
    }
    const auto ret = CapFor(0x0D);
    CHECK(caps[ret->cap].label == "RETURN");
    CHECK_FALSE(ret->ctrl);
    const auto bang = CapFor('!');
    CHECK(caps[bang->cap].label == "1");
    CHECK(bang->shift);
    const auto a = CapFor('A');
    CHECK(caps[a->cap].label == "A");
    CHECK_FALSE(a->shift);
    const auto nul = CapFor(0x00);
    CHECK(caps[nul->cap].label == "P");
    CHECK((nul->shift && nul->ctrl));
    CHECK_FALSE(CapFor(0x60));
    CHECK_FALSE(CapFor(0x7F));
}

TEST_CASE("keyboard layout: geometry", "[view]") {
    const auto caps = KeyCaps();
    int shifts = 0, ctrls = 0;
    for (std::size_t i = 0; i < caps.size(); ++i) {
        const KeyCap& a = caps[i];
        CHECK(a.x >= 0);
        CHECK(a.x + a.width <= kKeyboardWidth);
        CHECK(a.y >= 0);
        CHECK(a.y < kKeyboardRows);
        shifts += a.kind == CapKind::Shift ? 1 : 0;
        ctrls += a.kind == CapKind::Ctrl ? 1 : 0;
        for (std::size_t j = i + 1; j < caps.size(); ++j) {
            const KeyCap& b = caps[j];
            const bool overlap = a.y == b.y && a.x < b.x + b.width && b.x < a.x + a.width;
            CHECK_FALSE(overlap);
        }
    }
    CHECK(shifts == 2);
    CHECK(ctrls == 1);
}
