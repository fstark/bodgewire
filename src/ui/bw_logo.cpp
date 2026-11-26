#include "ui/bw_logo.hpp"

#include <cmath>
#include <numbers>
#include <vector>

namespace bw {

namespace {

// The geometry of docs/p8h-oscilloscope/mockups/scope_mock.py's logo("plain"), in its units.
constexpr float kWidth = 1000, kHeight = 622, kCornerRadius = 74;
constexpr float kCx = 500, kCy = 311, kRadius = 292;
constexpr float kStem = 56, kBar = 44, kTop = 196, kBottom = 425, kRound = 40;
constexpr float kB0 = 282, kB1 = 442;                        // the b
constexpr float kW0 = 472, kS1 = 562, kS2 = 652, kW1 = 708;  // the w and its three stems
constexpr float kGap = 18;                                   // white each side of the stem
constexpr float kScale = 0.94f;
const float kSlant = std::tan(-21 * std::numbers::pi_v<float> / 180);
constexpr ImU32 kBlue = IM_COL32(10, 85, 160, 255);
constexpr ImU32 kWhite = IM_COL32(255, 255, 255, 255);
constexpr int kCornerSteps = 6;

// An outline in the logo's upright units, built with straight lines and quadratic corners.
class Outline {
public:
    Outline& to(float x, float y) {
        points_.push_back({x, y});
        return *this;
    }
    // A quadratic corner from the last point through control (cx, cy) to (x, y).
    Outline& corner(float cx, float cy, float x, float y) {
        const ImVec2 p0 = points_.back();
        for (int i = 1; i <= kCornerSteps; ++i) {
            const float t = static_cast<float>(i) / kCornerSteps, u = 1 - t;
            points_.push_back({u * u * p0.x + 2 * u * t * cx + t * t * x,
                               u * u * p0.y + 2 * u * t * cy + t * t * y});
        }
        return *this;
    }
    // Filled, slanted and scaled about the circle's centre, onto the screen.
    void fill(ImDrawList& list, ImVec2 min, float k, ImU32 colour) const {
        std::vector<ImVec2> screen;
        screen.reserve(points_.size());
        for (const ImVec2 p : points_) {
            const float x = kCx + kScale * (p.x - kCx) + kSlant * kScale * (p.y - kCy);
            const float y = kCy + kScale * (p.y - kCy);
            screen.push_back({min.x + x * k, min.y + y * k});
        }
        list.AddConcavePolyFilled(screen.data(), static_cast<int>(screen.size()), colour);
    }

private:
    std::vector<ImVec2> points_;
};

}  // namespace

void DrawBwLogo(ImDrawList& list, ImVec2 min, float height) {
    const float k = height / kHeight;
    const ImVec2 max{min.x + kWidth * k, min.y + height};
    list.AddRectFilled(min, max, kBlue, kCornerRadius * k);
    list.AddCircleFilled({min.x + kCx * k, min.y + kCy * k}, kRadius * k, kWhite, 64);
    list.PushClipRect(min, max, true);
    // The stem's white gap, where it crosses the blue above the circle.
    Outline()
        .to(kB0 - kGap, -60)
        .to(kB0 + kStem + kGap, -60)
        .to(kB0 + kStem + kGap, kTop)
        .to(kB0 - kGap, kTop)
        .fill(list, min, k, kWhite);
    // The b: its stem from above the rectangle and its bowl, rounded on the right; then its
    // counter, white over it (a concave fill has no holes).
    Outline()
        .to(kB0, -60)
        .to(kB0 + kStem, -60)
        .to(kB0 + kStem, kTop)
        .to(kB1 - kRound, kTop)
        .corner(kB1, kTop, kB1, kTop + kRound)
        .to(kB1, kBottom - kRound)
        .corner(kB1, kBottom, kB1 - kRound, kBottom)
        .to(kB0, kBottom)
        .fill(list, min, k, kBlue);
    Outline()
        .to(kB0 + kStem, kTop + kBar)
        .to(kB1 - kStem, kTop + kBar)
        .to(kB1 - kStem, kBottom - kBar)
        .to(kB0 + kStem, kBottom - kBar)
        .fill(list, min, k, kWhite);
    // The w: three stems on a bar, the middle one shorter, rounded below.
    Outline()
        .to(kW0, kTop)
        .to(kW0 + kStem, kTop)
        .to(kW0 + kStem, kBottom - kBar)
        .to(kS1, kBottom - kBar)
        .to(kS1, kTop + 64)
        .to(kS1 + kStem, kTop + 64)
        .to(kS1 + kStem, kBottom - kBar)
        .to(kS2, kBottom - kBar)
        .to(kS2, kTop)
        .to(kW1, kTop)
        .to(kW1, kBottom - kRound)
        .corner(kW1, kBottom, kW1 - kRound, kBottom)
        .to(kW0 + kRound, kBottom)
        .corner(kW0, kBottom, kW0, kBottom - kRound)
        .fill(list, min, k, kBlue);
    list.PopClipRect();
}

}  // namespace bw
