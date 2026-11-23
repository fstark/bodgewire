#include "view/pace.hpp"

#include <format>

namespace bw {

namespace {

constexpr double kBehind = 0.9;
constexpr double kCaughtUp = 0.95;
constexpr auto kBehindAfter = std::chrono::seconds(2);
constexpr auto kShownFor = std::chrono::milliseconds(500);

}  // namespace

Pace PaceWatch::update(bool running, double ratio, Clock::time_point now) {
    if (!running) {
        pace_ = {};
        under_.reset();
        shownAt_.reset();
        return pace_;
    }
    if (!shownAt_ || now - *shownAt_ >= kShownFor) {
        pace_.shown = ratio;
        shownAt_ = now;
    }
    if (pace_.behind) {
        pace_.behind = ratio < kCaughtUp;
    } else if (ratio >= kBehind) {
        under_.reset();
    } else if (!under_) {
        under_ = now;
    } else if (now - *under_ >= kBehindAfter) {
        pace_.behind = true;
        under_.reset();
    }
    return pace_;
}

std::string SpeedText(double ratio) {
    return ratio < 10 ? std::format("{:.2f}×", ratio) : std::format("{:.0f}×", ratio);
}

}  // namespace bw
