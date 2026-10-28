#include "devices/bmp.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bw {

namespace {

void PutU16(std::ostream& out, std::uint16_t v) {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>(v >> 8));
}

void PutU32(std::ostream& out, std::uint32_t v) {
    PutU16(out, static_cast<std::uint16_t>(v & 0xFFFF));
    PutU16(out, static_cast<std::uint16_t>(v >> 16));
}

}  // namespace

void WriteGreyscaleBmp(std::ostream& out, const TvFrame& frame) {
    constexpr std::uint32_t kHeaders = 14 + 40;
    constexpr std::uint32_t kPalette = 256 * 4;
    constexpr std::uint32_t kImage = kTvColumns * kTvRows;  // stride 512 is a multiple of 4

    out.write("BM", 2);
    PutU32(out, kHeaders + kPalette + kImage);
    PutU32(out, 0);
    PutU32(out, kHeaders + kPalette);

    PutU32(out, 40);
    PutU32(out, kTvColumns);
    PutU32(out, kTvRows);  // positive: bottom-up
    PutU16(out, 1);        // planes
    PutU16(out, 8);        // bits per pixel
    PutU32(out, 0);        // no compression
    PutU32(out, kImage);
    PutU32(out, 2835);  // 72 dpi
    PutU32(out, 2835);
    PutU32(out, 256);
    PutU32(out, 0);

    for (int i = 0; i < 256; ++i) {
        const char grey = static_cast<char>(i);
        out.put(grey).put(grey).put(grey).put(0);
    }
    for (int row = kTvRows - 1; row >= 0; --row) {
        for (int column = 0; column < kTvColumns; ++column) {
            const float cell = std::clamp(frame.at(row, column), 0.0f, 1.0f);
            out.put(static_cast<char>(std::lround(cell * 255.0f)));
        }
    }
}

}  // namespace bw
