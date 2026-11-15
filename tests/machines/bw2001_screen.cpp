#include "bw2001_screen.hpp"

#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "parts/rom_contents.hpp"

namespace bw {

std::span<const std::uint8_t> Bw2001CharacterRom() {
    static const std::vector<std::uint8_t> rom = [] {
        const auto words =
            ReadRomContents(std::filesystem::path(BW_ROOT_DIR) / "roms" / "901447-08.bin", 2048, 8);
        REQUIRE(words.has_value());
        return *words;
    }();
    return rom;
}

std::array<std::uint8_t, 8> Bw2001CellDots(const TvFrame& frame, int cell) {
    const int row = cell / kBw2001Columns, column = cell % kBw2001Columns;
    std::array<std::uint8_t, 8> dots{};
    for (int line = 0; line < 8; ++line) {
        for (int dot = 0; dot < 8; ++dot) {
            if (frame.at(kBw2001TextTop + row * 8 + line, kBw2001TextLeft + column * 8 + dot) >=
                0.5f) {
                dots[static_cast<std::size_t>(line)] |= static_cast<std::uint8_t>(0x80 >> dot);
            }
        }
    }
    return dots;
}

std::array<std::uint8_t, 8> Bw2001Glyph(std::uint8_t code) {
    const auto rom = Bw2001CharacterRom();
    std::array<std::uint8_t, 8> glyph{};
    for (std::size_t line = 0; line < 8; ++line) {
        const std::uint8_t dots = rom[(code & 0x7Fu) * 8u + line];
        glyph[line] = (code & 0x80) != 0 ? static_cast<std::uint8_t>(~dots) : dots;
    }
    return glyph;
}

int Bw2001BorderDots(const TvFrame& frame) {
    int lit = 0;
    for (int row = 0; row < kTvRows; ++row) {
        for (int column = 0; column < kTvColumns; ++column) {
            const bool inside = row >= kBw2001TextTop && row < kBw2001TextTop + 8 * kBw2001Rows &&
                                column >= kBw2001TextLeft &&
                                column < kBw2001TextLeft + 8 * kBw2001Columns;
            lit += !inside && frame.at(row, column) >= 0.5f ? 1 : 0;
        }
    }
    return lit;
}

std::vector<std::string> Bw2001TvRows(const TvFrame& frame) {
    std::vector<std::string> rows;
    for (int row = 0; row < kBw2001Rows; ++row) {
        std::string text;
        for (int column = 0; column < kBw2001Columns; ++column) {
            const auto dots = Bw2001CellDots(frame, row * kBw2001Columns + column);
            char c = '?';
            for (unsigned code = 0; code < 0x40; ++code) {
                if (dots == Bw2001Glyph(static_cast<std::uint8_t>(code)) ||
                    dots == Bw2001Glyph(static_cast<std::uint8_t>(code | 0x80))) {
                    c = static_cast<char>(code < 0x20 ? code + 0x40 : code);
                    break;
                }
            }
            text.push_back(c);
        }
        text.erase(text.find_last_not_of(' ') + 1);
        rows.push_back(text);
    }
    while (!rows.empty() && rows.back().empty()) {
        rows.pop_back();
    }
    return rows;
}

}  // namespace bw
