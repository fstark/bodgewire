#include "view/number_pad.hpp"

#include <algorithm>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

// `keys` typed one after the other from `text`, the first on a fresh field.
std::string Typed(std::string text, std::initializer_list<std::string_view> keys, PadKind kind,
                  std::size_t limit = 8) {
    bool fresh = true;
    for (const std::string_view key : keys) {
        text = PadTyped(std::move(text), key, kind, limit, fresh);
        fresh = false;
    }
    return text;
}

}  // namespace

TEST_CASE("number pad: the keys, four a row", "[view]") {
    for (const PadKind kind : {PadKind::Hex, PadKind::Pattern, PadKind::Duration}) {
        const auto keys = PadKeys(kind);
        CHECK(keys.size() % kPadColumns == 0);
        CHECK(std::ranges::count(keys, kPadBackspace) == 1);
        CHECK(std::ranges::count(keys, kPadConfirm) >= 1);
    }
    CHECK(std::ranges::count(PadKeys(PadKind::Hex), "x") == 0);
    CHECK(std::ranges::count(PadKeys(PadKind::Pattern), "x") == 1);
    CHECK(std::ranges::count_if(PadKeys(PadKind::Duration), IsPadUnit) == 3);
    CHECK_FALSE(IsPadUnit("OK"));
}

TEST_CASE("number pad: the first key replaces the field's value, the next ones add", "[view]") {
    CHECK(Typed("E000", {"1"}, PadKind::Hex) == "1");
    CHECK(Typed("E000", {"1", "F", "0"}, PadKind::Hex) == "1F0");
    CHECK(Typed("", {"A", "B"}, PadKind::Hex) == "AB");
    CHECK(Typed("12", {"x"}, PadKind::Hex) == "12");  // no x in an address: unchanged
    CHECK(Typed("E0xx", {"E", "0", "x", "x"}, PadKind::Pattern) == "E0xx");
    CHECK(Typed("", {"G"}, PadKind::Pattern).empty());
}

TEST_CASE("number pad: backspace", "[view]") {
    CHECK(Typed("E000", {"<"}, PadKind::Hex).empty());  // fresh: the whole value goes
    CHECK(Typed("", {"1", "2", "<"}, PadKind::Hex) == "1");
    CHECK(Typed("", {"1", "<", "<", "<"}, PadKind::Hex).empty());
}

TEST_CASE("number pad: the limit", "[view]") {
    CHECK(Typed("", {"1", "2", "3", "4", "5"}, PadKind::Hex, 4) == "1234");
    CHECK(Typed("", {"1", "2", "3", "4", "<", "5"}, PadKind::Hex, 4) == "1235");
}

TEST_CASE("number pad: a time, one point, a unit", "[view]") {
    CHECK(Typed("3.2 \xc2\xb5s", {"1", "2", ".", "5"}, PadKind::Duration) == "12.5");
    CHECK(Typed("", {"1", ".", ".", "5"}, PadKind::Duration) == "1.5");
    CHECK(Typed("", {".", "5"}, PadKind::Duration) == "0.5");
    CHECK(Typed("", {"A"}, PadKind::Duration).empty());
    CHECK(Typed("", {"1", "2", ".", "5", "us"}, PadKind::Duration) == "12.5us");
    CHECK(Typed("", {"2", "ms", "5"}, PadKind::Duration) == "2ms");  // nothing after its unit
    // A unit on a fresh field keeps its number: 3.2 µs made 3.2 ns.
    CHECK(Typed("3.2 \xc2\xb5s", {"ns"}, PadKind::Duration) == "3.2ns");
    CHECK(Typed("", {"ns"}, PadKind::Duration).empty());  // no number: nothing
    CHECK(Typed("12", {"ns"}, PadKind::Hex) == "12");     // no units on an address
}
