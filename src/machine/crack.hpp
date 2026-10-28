#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>

#include "board/board.hpp"
#include "machine/puzzle.hpp"

namespace bw {

// Straight Trace on `layer` whose centre line is nearest to `at`, within half its width.
std::expected<std::size_t, std::string> FindCrackTrace(const Board& board, Layer layer, Point at);

// `board` with each crack cut as a gap wider than the Trace; Check()s every crack is on a Trace.
Board CrackBoard(const Board& board, std::span<const CrackDecl> cracks);

std::string FormatMillimetres(std::int64_t nm);  // "52.30"
std::string FormatPoint(Layer layer, Point at);  // "F.Cu 52.30 41.00"

}  // namespace bw
