#pragma once

#include <cstdint>

namespace bw {

// What of a failing letter still glows: a dying tube often lights at one end only.
enum class NeonPart : std::uint8_t { Whole, LeftHalf, RightHalf };

// The launcher's neon title at a moment (2026-11-25): the supply's brightness, and at most one
// failing letter's. 1: lit as always; 0: dark, the unlit glass.
struct NeonSign {
    float sign = 1;                  // a brownout dims every letter
    int letter = -1;                 // the failing letter, an index into the title; -1: none
    float letterBrightness = 1;      // the failing letter's, under the sign's
    NeonPart lit = NeonPart::Whole;  // the failing letter's part at letterBrightness, the rest dark
};

// The title `seconds` after the program started, from the time and `run` alone (no state; `run`, a
// number for this run of the program, so each run fails differently). The tubes strike within 0.4 s
// and stay steady until 4 s, so a --screenshot is as ever. Then, in each 8 s, an event at random:
// one time in four a letter fails (it stutters, stays dark or half lit 1 to 2 s, and strikes back);
// one in eight the supply dips once or twice. The whole sign never flashes more than twice a second
// (the photosensitivity guideline: 3).
NeonSign NeonSignAt(double seconds, int letters, std::uint64_t run = 0);

}  // namespace bw
