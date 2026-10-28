#include "devices/bmp.hpp"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace bw;

namespace {

std::uint32_t U32At(const std::string& s, std::size_t offset) {
    std::uint32_t v = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        v |= static_cast<std::uint32_t>(static_cast<unsigned char>(s[offset + i])) << (8 * i);
    }
    return v;
}

std::string Bmp(const TvFrame& frame) {
    std::ostringstream out;
    WriteGreyscaleBmp(out, frame);
    return out.str();
}

}  // namespace

TEST_CASE("bmp: header and size", "[devices]") {
    const std::string bmp = Bmp(TvFrame{});
    REQUIRE(bmp.size() == 14 + 40 + 1024 + 163'840);
    CHECK(bmp.substr(0, 2) == "BM");
    CHECK(U32At(bmp, 2) == bmp.size());
    CHECK(U32At(bmp, 10) == 1078);
    CHECK(U32At(bmp, 18) == 512);
    CHECK(U32At(bmp, 22) == 320);
    CHECK((U32At(bmp, 28) & 0xFFFF) == 8);
}

TEST_CASE("bmp: rows are bottom-up", "[devices]") {
    TvFrame frame;
    frame.cells[0] = 1.0f;
    frame.cells[1] = 0.5f;
    const std::string bmp = Bmp(frame);
    const std::size_t top = 1078 + std::size_t{319} * 512;
    CHECK(static_cast<unsigned char>(bmp[top]) == 255);
    CHECK(static_cast<unsigned char>(bmp[top + 1]) == 128);
    CHECK(static_cast<unsigned char>(bmp[1078]) == 0);
}
