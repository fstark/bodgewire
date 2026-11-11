#include "machine/typing.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace bw;
using namespace bw::literals;

TEST_CASE("typing: reference, start and keys", "[machine]") {
    const auto hi = ParseTyping("J1:200ms:Hi");
    REQUIRE(hi.has_value());
    CHECK(hi->reference == "J1");
    CHECK(hi->start == 200_ms);
    CHECK(hi->keys == std::vector<std::uint8_t>{0x48, 0x49});
    CHECK(hi->at(1) == 300_ms);

    const auto colons = ParseTyping("J1:1ms:a:b, c");
    REQUIRE(colons.has_value());
    CHECK(colons->keys == std::vector<std::uint8_t>{'A', ':', 'B', ',', ' ', 'C'});

    const auto escapes = ParseTyping(R"(J1:1ms:A\nB\b\\)");
    REQUIRE(escapes.has_value());
    CHECK(escapes->keys == std::vector<std::uint8_t>{'A', 0x0D, 'B', 0x08, '\\'});
}

TEST_CASE("typing: refusals", "[machine]") {
    for (const char* text : {"J1", "J1:200ms", ":1ms:A", "J1:x:A", "J1:1ms:"}) {
        INFO(text);
        const auto typing = ParseTyping(text);
        REQUIRE_FALSE(typing.has_value());
        CHECK(typing.error() == "bad typing '" + std::string(text) + "' (expected REF:START:TEXT)");
    }
    CHECK(ParseTyping("J1:1ms:{").error() == "cannot type '{' on a keyboard");
    CHECK(ParseTyping(R"(J1:1ms:\q)").error() == "bad escape '\\q'");
    CHECK(ParseTyping(R"(J1:1ms:A\)").error() == "bad escape '\\' at the end");
}
