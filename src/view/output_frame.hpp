#pragma once

#include <cstdint>

#include "view/board_scene.hpp"

namespace bw {

inline constexpr std::int64_t kFrameMargin = 30'000'000;      // nm of Board around the lamps
inline constexpr std::int64_t kSmallBoardSide = 130'000'000;  // nm: both sides at most: whole
inline constexpr double kWholeShare = 0.6;                    // a frame covering more: whole

// The part of a Machine's Board the launcher shows (P7b): its lamps' courtyards (BoardScene::lamp:
// every LED and digit), kFrameMargin around them, clamped to the Board's bounds. The Board's
// bounds instead when it has a TV, has no lamp, is small (both sides ≤ kSmallBoardSide), or when
// the frame's area is more than kWholeShare of the Board's.
Bounds OutputFrame(const BoardScene& scene, bool hasTv);

}  // namespace bw
