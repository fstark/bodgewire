#include "ui/title.hpp"

#include <array>
#include <cfloat>
#include <cmath>
#include <numbers>

#include "ui/bench.hpp"

namespace bw {

namespace {

constexpr const char* kTitle = "BODGEWIRE";
constexpr ImU32 kCore = IM_COL32(239, 254, 255, 255);
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

}  // namespace

float DrawTitle(ImDrawList& list, ImFont* font, ImVec2 centreTop, float size) {
    if (font == nullptr) {
        return 0;
    }
    SharpFont(font);
    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0, kTitle);
    const ImVec2 at{std::round(centreTop.x - extent.x / 2), std::round(centreTop.y)};
    for (const Ring& ring : kRings) {
        const ImU32 colour = IM_COL32(kGlowR, kGlowG, kGlowB, ring.alpha);
        for (int i = 0; i < ring.copies; ++i) {
            const float angle = 2 * std::numbers::pi_v<float> * static_cast<float>(i) /
                                static_cast<float>(ring.copies);
            list.AddText(
                font, size,
                {at.x + ring.radius * std::cos(angle), at.y + ring.radius * std::sin(angle)},
                colour, kTitle);
        }
    }
    list.AddText(font, size, at, kCore, kTitle);
    return extent.x;
}

}  // namespace bw
