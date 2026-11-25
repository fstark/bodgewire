#include "view/pace.hpp"

#include <algorithm>
#include <cmath>
#include <format>

namespace bw {

namespace {

constexpr double kBehind = 0.9;
constexpr double kCaughtUp = 0.95;
constexpr auto kBehindAfter = std::chrono::seconds(2);
constexpr auto kShownFor = std::chrono::milliseconds(500);

constexpr auto kHeadroomWindow = std::chrono::seconds(1);    // the band's measure
constexpr auto kHeadroomShownFor = std::chrono::seconds(2);  // the number's, and how long it holds
constexpr auto kHeadroomFirst = std::chrono::milliseconds(900);  // measured over at least this
constexpr double kSlowUnder = 0.9;                               // Playable → Slow
constexpr double kPlayableOver = 1.0;                            // → Playable
constexpr double kTooSlowUnder = 0.27;                           // → TooSlow
constexpr double kSlowOver = 0.3;                                // TooSlow → Slow

Playability NextBand(Playability band, double ratio) {
    if (ratio < kTooSlowUnder) {
        return Playability::TooSlow;
    }
    if (ratio > kPlayableOver) {
        return Playability::Playable;
    }
    if (band == Playability::TooSlow) {
        return ratio > kSlowOver ? Playability::Slow : Playability::TooSlow;
    }
    return ratio < kSlowUnder ? Playability::Slow : band;
}

}  // namespace

Pace PaceWatch::update(bool running, double ratio, Clock::time_point now, double target) {
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
        pace_.behind = ratio < kCaughtUp * target;
    } else if (ratio >= kBehind * target) {
        under_.reset();
    } else if (!under_) {
        under_ = now;
    } else if (now - *under_ >= kBehindAfter) {
        pace_.behind = true;
        under_.reset();
    }
    return pace_;
}

void Headroom::update(Time simTime, std::int64_t runNs, Clock::time_point now) {
    if (!history_.empty() && (simTime < history_.back().simTime || runNs < history_.back().runNs)) {
        reset();
    }
    history_.push_back({now, simTime, runNs});
    while (now - history_.front().wall > kHeadroomShownFor) {
        history_.pop_front();
    }
    // Simulated time over work since the oldest Point within `window`; none on too short a span.
    const auto ratioOver = [&](Clock::duration window) -> std::optional<double> {
        const auto first = std::ranges::find_if(
            history_, [&](const Point& point) { return now - point.wall <= window; });
        const std::int64_t work = runNs - first->runNs;
        if (now - first->wall < kHeadroomFirst || work <= 0) {
            return std::nullopt;
        }
        return static_cast<double>(simTime - first->simTime) /
               static_cast<double>(kPicosPerSecond) / (static_cast<double>(work) * 1e-9);
    };
    const auto ratio = ratioOver(kHeadroomWindow);
    if (!ratio) {
        return;
    }
    const Playability before = band_;
    band_ = NextBand(band_, *ratio);
    // The number held 2 s, over the last 2 s; at once when the band changes (its sentence with it).
    if (!shownAt_ || now - *shownAt_ >= kHeadroomShownFor || band_ != before) {
        shown_ = band_ != before ? *ratio : ratioOver(kHeadroomShownFor).value_or(*ratio);
        shownAt_ = now;
    }
}

void Headroom::reset() { *this = {}; }

// No more digits than the measure is worth: "0.34×", "1.2×", "13×".
std::string HeadroomNumber(double ratio) {
    if (ratio < 0.995) {
        return std::format("{:.2f}×", ratio);
    }
    if (ratio < 3.95) {
        return std::format("{:.1f}×", ratio);
    }
    return std::format("{:.0f}×", ratio);
}

std::string HeadroomText(double ratio, Playability band) {
    if (band == Playability::Playable) {
        return std::format("On this computer it runs at full speed: it could go {} as fast as the "
                           "real one.",
                           HeadroomNumber(ratio));
    }
    const long percent = std::clamp(std::lround(ratio * 100), 1L, 99L);
    return std::format("On this computer it runs at {}% of the real one's speed.", percent);
}

std::string SpeedText(double ratio) {
    return ratio < 10 ? std::format("{:.2f}×", ratio) : std::format("{:.0f}×", ratio);
}

}  // namespace bw
