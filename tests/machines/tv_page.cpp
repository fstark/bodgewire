#include "tv_page.hpp"

#include <cmath>
#include <cstddef>

namespace bw {

namespace {

// One dot is 153.85 ns (6.5 MHz), 1.23 TV columns of 125 ns; one line one row.
constexpr double kColumnsPerDot = 1e12 / 6.5e6 / static_cast<double>(Tv::kColumnTime);
constexpr int kCellDots = 8, kCellLines = 8;

int Column(double left, int dot) {
    return static_cast<int>(std::floor(left + (dot + 0.5) * kColumnsPerDot));
}

}  // namespace

PageDots ExpectedPage(std::span<const std::uint8_t> glyphs, int columns, int rows,
                      const std::function<unsigned(int row, int column)>& code) {
    PageDots dots(static_cast<std::size_t>(rows * kCellLines),
                  std::vector<bool>(static_cast<std::size_t>(columns * kCellDots), false));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const unsigned c = code(row, column);
            for (int line = 0; line < kCellLines; ++line) {
                const unsigned glyph = glyphs[(c & 0x3F) * 8 + static_cast<unsigned>(line)];
                for (int x = 0; x < kCellDots; ++x) {
                    const bool ink = ((glyph >> (7 - x)) & 1) != 0;
                    dots[static_cast<std::size_t>(row * kCellLines + line)]
                        [static_cast<std::size_t>(column * kCellDots + x)] =
                            ink != ((c & 0x80) != 0);
                }
            }
        }
    }
    return dots;
}

bool Matches(const TvFrame& frame, const PageDots& page, double left, int top) {
    const int lines = static_cast<int>(page.size());
    const int dots = static_cast<int>(page.front().size());
    for (int y = 0; y < lines; ++y) {
        for (int x = 0; x < dots; ++x) {
            const int inCell = x % kCellDots;
            if (inCell < 1 || inCell > 5) {
                continue;
            }
            const int column = Column(left, x);
            if (top + y >= kTvRows || column >= kTvColumns) {
                return false;
            }
            const bool ink = frame.at(top + y, column) < 0.5f;
            if (ink != page[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]) {
                return false;
            }
        }
    }
    return true;
}

std::optional<std::pair<double, int>> FindPage(const TvFrame& frame, const PageDots& page) {
    const int lines = static_cast<int>(page.size());
    const int dots = static_cast<int>(page.front().size());
    for (int top = 0; top + lines <= kTvRows; ++top) {
        for (double left = 0; Column(left, dots - 1) < kTvColumns; left += 0.25) {
            if (Matches(frame, page, left, top)) {
                return std::pair{left, top};
            }
        }
    }
    return std::nullopt;
}

int InkCount(const TvFrame& frame) {
    int count = 0;
    for (int row = 0; row < kTvRows; ++row) {
        for (int column = 0; column < kTvColumns; ++column) {
            count += frame.at(row, column) < 0.5f ? 1 : 0;
        }
    }
    return count;
}

}  // namespace bw
