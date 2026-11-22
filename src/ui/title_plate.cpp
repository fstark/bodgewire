#include "ui/title_plate.hpp"

#include <array>
#include <span>
#include <vector>

#include "board/board.hpp"
#include "ui/board_look.hpp"

namespace bw {

namespace {

// Letters on a grid of units: 6 tall, 4 wide (the I 3: room for its wire), 45° bends only, like a
// router's traces.
struct Stroke {
    std::vector<ImVec2> points;
    bool closed = false;  // a loop: no pads
};
struct Glyph {
    float width;
    std::vector<Stroke> strokes;
};

constexpr float kLetterHeight = 6;
constexpr float kLetterGap = 2;
constexpr float kPadX = 3;  // units from the plate's sides to the letters
constexpr float kPadY = 2;  // above and below them
constexpr float kTraceWidth = 0.55f;
constexpr float kPadRadius = 0.62f;
constexpr float kHoleRadius = 0.24f;
// The Board's real look (board_look.cpp, board_view.cpp): its mask, its tinned pads and holes,
// a bodge wire's blue insulation.
constexpr ImU32 kMask = IM_COL32(6, 52, 28, 255);
constexpr ImU32 kMaskEdge = IM_COL32(40, 92, 58, 255);
constexpr ImU32 kTin = IM_COL32(205, 205, 195, 255);
constexpr ImU32 kHole = IM_COL32(12, 12, 12, 255);
constexpr ImU32 kWire = IM_COL32(120, 170, 255, 255);
constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 110);

const std::vector<Glyph>& Word() {
    // B O D G E W I R E
    static const std::vector<Glyph> word{
        {4,
         {{{{0, 0}, {0, 6}}},
          {{{0, 0}, {3, 0}, {4, 1}, {4, 2}, {3, 3}, {0, 3}}},
          {{{0, 3}, {3, 3}, {4, 4}, {4, 5}, {3, 6}, {0, 6}}}}},
        {4, {{{{1, 0}, {3, 0}, {4, 1}, {4, 5}, {3, 6}, {1, 6}, {0, 5}, {0, 1}}, true}}},
        {4, {{{{0, 0}, {3, 0}, {4, 1}, {4, 5}, {3, 6}, {0, 6}}, true}}},
        {4, {{{{4, 1}, {3, 0}, {1, 0}, {0, 1}, {0, 5}, {1, 6}, {3, 6}, {4, 5}, {4, 3}, {2, 3}}}}},
        {4, {{{{4, 0}, {0, 0}, {0, 6}, {4, 6}}}, {{{0, 3}, {3, 3}}}}},
        {4, {{{{0, 0}, {0, 6}, {2, 4}, {4, 6}, {4, 0}}}}},
        {3, {{{{0.5f, 0}, {0.5f, 6}}}}},  // cracked: the bodge wire beside it mends it
        {4,
         {{{{0, 6}, {0, 0}, {3, 0}, {4, 1}, {4, 2}, {3, 3}, {0, 3}}}, {{{2, 3}, {4, 5}, {4, 6}}}}},
        {4, {{{{4, 0}, {0, 0}, {0, 6}, {4, 6}}}, {{{0, 3}, {3, 3}}}}},
    };
    return word;
}

constexpr std::size_t kBrokenLetter = 6;  // the I

float WordWidth() {
    float units = 0;
    for (const Glyph& glyph : Word()) {
        units += glyph.width + kLetterGap;
    }
    return units - kLetterGap;
}

}  // namespace

float DrawTitlePlate(ImDrawList& list, ImVec2 centreTop, float height) {
    const float u = height / (kLetterHeight + 2 * kPadY);
    const float width = (WordWidth() + 2 * kPadX) * u;
    const ImVec2 min{centreTop.x - width / 2, centreTop.y};
    const ImVec2 max{min.x + width, min.y + height};
    list.AddRectFilled({min.x + 3, min.y + 4}, {max.x + 3, max.y + 4}, kShadow, u);
    list.AddRectFilled(min, max, kMask, u);
    list.AddRect(min, max, kMaskEdge, u, 0, 1.0f);
    for (const ImVec2 corner :
         {ImVec2{min.x + 1.3f * u, min.y + 1.3f * u}, ImVec2{max.x - 1.3f * u, min.y + 1.3f * u},
          ImVec2{min.x + 1.3f * u, max.y - 1.3f * u}, ImVec2{max.x - 1.3f * u, max.y - 1.3f * u}}) {
        list.AddCircleFilled(corner, 0.75f * u, kTin);
        list.AddCircleFilled(corner, 0.45f * u, kHole);
    }

    const ImU32 copper = TraceColour(Layer::Front);
    std::vector<ImVec2> pads;
    float x = min.x + kPadX * u;
    const float top = min.y + kPadY * u;
    ImVec2 wireA{}, wireB{};
    for (std::size_t g = 0; g < Word().size(); ++g) {
        const Glyph& glyph = Word()[g];
        for (const Stroke& stroke : glyph.strokes) {
            std::vector<ImVec2> screen;
            for (const ImVec2 p : stroke.points) {
                screen.push_back({x + p.x * u, top + p.y * u});
            }
            list.AddPolyline(screen.data(), static_cast<int>(screen.size()), copper,
                             stroke.closed ? ImDrawFlags_Closed : ImDrawFlags_None,
                             kTraceWidth * u);
            if (!stroke.closed) {
                pads.push_back(screen.front());
                pads.push_back(screen.back());
            }
        }
        if (g == kBrokenLetter) {
            wireA = {x + 0.5f * u, top};
            wireB = {x + 0.5f * u, top + kLetterHeight * u};
            // The crack: a sliver of mask across the trace, a little below the middle.
            list.AddLine({wireA.x - 0.5f * u, top + 3.4f * u}, {wireA.x + 0.5f * u, top + 3.1f * u},
                         kMask, 0.22f * u);
        }
        x += (glyph.width + kLetterGap) * u;
    }
    for (const ImVec2 pad : pads) {
        list.AddCircleFilled(pad, kPadRadius * u, kTin);
        list.AddCircleFilled(pad, kHoleRadius * u, kHole);
    }
    // The bodge wire: out of the I's top pad, bowing right of the crack, into its bottom pad, its
    // shadow under it.
    const ImVec2 c1{wireA.x + 2.6f * u, wireA.y + 0.8f * u};
    const ImVec2 c2{wireB.x + 2.6f * u, wireB.y - 0.8f * u};
    const float wire = 0.42f * u;
    list.AddBezierCubic({wireA.x + 1, wireA.y + 2}, {c1.x + 1, c1.y + 2}, {c2.x + 1, c2.y + 2},
                        {wireB.x + 1, wireB.y + 2}, kShadow, wire);
    list.AddBezierCubic(wireA, c1, c2, wireB, kWire, wire);
    for (const ImVec2 end : {wireA, wireB}) {
        list.AddCircleFilled(end, 0.38f * u, kTin);  // the solder over the wire's end
    }
    return width;
}

}  // namespace bw
