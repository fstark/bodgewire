#include "ui/title.hpp"

#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <vector>

#include "ui/bench.hpp"
#include "view/neon_sign.hpp"

namespace bw {

namespace {

constexpr const char* kTitle = "BODGEWIRE";
constexpr int kLetters = 9;
constexpr int kCoreR = 239, kCoreG = 254, kCoreB = 255;
constexpr ImU32 kGlass = IM_COL32(52, 64, 70, 255);     // a tube unlit, under one not fully lit
constexpr float kMargin = 4;                            // px: a half letter's clip past its box
constexpr int kGlowR = 63, kGlowG = 242, kGlowB = 255;  // #3ff2ff
// The glow: the text again around the core, rings of copies, fainter further out (as CSS's
// text-shadow 0 0 6/14/28 px).
struct Ring {
    float radius;  // px
    int copies;
    int alpha;  // each copy's, of 255
};
constexpr std::array<Ring, 7> kRings{{{18, 36, 2},
                                      {14, 32, 3},
                                      {10, 28, 5},
                                      {7, 24, 8},
                                      {4.5f, 18, 14},
                                      {2.5f, 14, 26},
                                      {1, 10, 60}}};

// Part of the title, from `begin` to `end`, `at` its top left, lit at `brightness` (0 to 1).
// `clip`: the half of a letter whose core glows, if only one; its glow, unclipped (a cut would
// show), is half as bright; `dark`: the other half, its unlit glass drawn over that glow.
struct Piece {
    const char* begin;
    const char* end;
    ImVec2 at;
    float brightness;
    std::optional<ImVec4> clip;
    std::optional<ImVec4> dark;
};

void PushClip(ImDrawList& list, const ImVec4& rect) {
    list.PushClipRect({rect.x, rect.y}, {rect.z, rect.w}, true);
}

int Alpha(int alpha, float brightness) {
    return static_cast<int>(std::lround(static_cast<float>(alpha) * brightness));
}

// The glow: the text again around the core, rings of copies (`core`: false), then the core.
void DrawLit(ImDrawList& list, ImFont* font, float size, const Piece& piece, bool core) {
    if (piece.brightness <= 0) {
        return;
    }
    if (core) {
        if (piece.dark) {
            PushClip(list, *piece.dark);
            list.AddText(font, size, piece.at, kGlass, piece.begin, piece.end);
            list.PopClipRect();
        }
        if (piece.clip) {
            PushClip(list, *piece.clip);
        }
        list.AddText(font, size, piece.at,
                     IM_COL32(kCoreR, kCoreG, kCoreB, Alpha(255, piece.brightness)), piece.begin,
                     piece.end);
        if (piece.clip) {
            list.PopClipRect();
        }
    } else {
        const float brightness = piece.clip ? piece.brightness / 2 : piece.brightness;
        for (const Ring& ring : kRings) {
            const ImU32 colour = IM_COL32(kGlowR, kGlowG, kGlowB, Alpha(ring.alpha, brightness));
            for (int i = 0; i < ring.copies; ++i) {
                const float angle = 2 * std::numbers::pi_v<float> * static_cast<float>(i) /
                                    static_cast<float>(ring.copies);
                list.AddText(font, size,
                             {piece.at.x + ring.radius * std::cos(angle),
                              piece.at.y + ring.radius * std::sin(angle)},
                             colour, piece.begin, piece.end);
            }
        }
    }
}

}  // namespace

float DrawTitle(ImDrawList& list, ImFont* font, ImVec2 centreTop, float size) {
    if (font == nullptr) {
        return 0;
    }
    SharpFont(font);
    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0, kTitle);
    const ImVec2 at{std::round(centreTop.x - extent.x / 2), std::round(centreTop.y)};
    const char* const end = kTitle + kLetters;
    // This run's failures (each run's own; the first 4 s steady whatever the run).
    static const auto run =
        static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    const NeonSign sign = NeonSignAt(ImGui::GetTime(), kLetters, run);
    std::vector<Piece> pieces;
    if (sign.letter < 0) {
        pieces.push_back({kTitle, end, at, sign.sign, {}, {}});
    } else {
        // Before the failing letter, the letter, after it (no kerning: the widths add up).
        const char* const letter = kTitle + sign.letter;
        const float x0 = at.x + font->CalcTextSizeA(size, FLT_MAX, 0, kTitle, letter).x;
        const float x1 = x0 + font->CalcTextSizeA(size, FLT_MAX, 0, letter, letter + 1).x;
        const float top = at.y - kMargin;
        const float bottom = at.y + extent.y + kMargin;
        const float middle = std::round((x0 + x1) / 2);
        const ImVec4 left{x0 - kMargin, top, middle, bottom};
        const ImVec4 right{middle, top, x1 + kMargin, bottom};
        std::optional<ImVec4> half;
        std::optional<ImVec4> dark;
        if (sign.lit == NeonPart::LeftHalf) {
            half = left;
            dark = right;
        } else if (sign.lit == NeonPart::RightHalf) {
            half = right;
            dark = left;
        }
        pieces.push_back({kTitle, letter, at, sign.sign, {}, {}});
        pieces.push_back(
            {letter, letter + 1, {x0, at.y}, sign.sign * sign.letterBrightness, half, dark});
        pieces.push_back({letter + 1, end, {x1, at.y}, sign.sign, {}, {}});
    }
    // Fully lit, the glass is not drawn: the title as it always was.
    if (sign.sign < 1 || (sign.letter >= 0 && sign.letterBrightness < 1) ||
        sign.lit != NeonPart::Whole) {
        list.AddText(font, size, at, kGlass, kTitle, end);
    }
    for (const bool core : {false, true}) {  // every glow under every core
        for (const Piece& piece : pieces) {
            DrawLit(list, font, size, piece, core);
        }
    }
    return extent.x;
}

}  // namespace bw
