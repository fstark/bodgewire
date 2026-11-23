#include "runner/channel_nets.hpp"

#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

TEST_CASE("channel nets: distinct Nets with the Channels each feeds", "[runner]") {
    const NetId a{3}, b{7};
    const std::array<std::optional<NetId>, 4> channels{a, std::nullopt, b, a};
    ChannelNets<std::uint8_t> nets;
    nets.adopt(channels);
    REQUIRE(nets.nets().size() == 2);
    CHECK(nets.nets()[0] == a);
    CHECK(nets.nets()[1] == b);
    CHECK(nets.maskOf(a) == 0b1001);
    CHECK(nets.maskOf(b) == 0b0100);
    CHECK(nets.maskOf(NetId{9}) == 0);
    nets.adopt(std::array<std::optional<NetId>, 4>{});
    CHECK(nets.nets().empty());
    CHECK(nets.maskOf(a) == 0);
}

TEST_CASE("channel nets: a 64-Channel mask reaches the last Channel", "[runner]") {
    std::array<std::optional<NetId>, 64> channels{};
    channels[63] = NetId{1};
    ChannelNets<std::uint64_t> nets;
    nets.adopt(channels);
    CHECK(nets.maskOf(NetId{1}) == std::uint64_t{1} << 63);
}
