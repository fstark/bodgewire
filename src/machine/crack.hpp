#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>

#include "board/board.hpp"

namespace bw {

// A placed crack: a point on a straight Trace's centre line.
struct Crack {
    Layer layer;
    Point at;

    bool operator==(const Crack&) const = default;
};

// Crack `percent` (0–100) along the crackable Copper from the pad under `a` to the pad under `b`
// (pad centres). Errors: "pads not joined by Copper", "no crackable Trace between the pads",
// "does not separate the pads".
std::expected<Crack, std::string> PlaceCrack(const Board& board, Point a, Point b, double percent);

// Straight Trace on `layer` whose centre line is nearest to `at`, within half its width.
std::expected<std::size_t, std::string> FindCrackTrace(const Board& board, Layer layer, Point at);

// `board` with each crack cut as a gap wider than the Trace; Check()s every crack is on a Trace.
Board CrackBoard(const Board& board, std::span<const Crack> cracks);

std::string FormatMillimetres(std::int64_t nm);  // "52.30"
std::string FormatPoint(Layer layer, Point at);  // "F.Cu 52.30 41.00"

}  // namespace bw
