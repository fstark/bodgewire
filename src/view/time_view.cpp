#include "view/time_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "view/time_text.hpp"

namespace bw {

namespace {

constexpr float kTextPad = 4;  // px between a value's text and its run's ends

Time Scaled(Time t, double factor) {
    return static_cast<Time>(std::llround(static_cast<double>(t) * factor));
}

}  // namespace

Time StepAtLeast(Time t) {
    for (Time decade = 1;; decade *= 10) {
        for (const Time m : {1, 2, 5}) {
            if (decade * m >= t) {
                return decade * m;
            }
        }
    }
}

Time StepUp(Time step) { return StepAtLeast(step + 1); }

Time StepDown(Time step) {
    Time below = 1;
    for (Time s = 1; s < step; s = StepUp(s)) {
        below = s;
    }
    return below;
}

TimeView ZoomAbout(TimeView view, Time anchor, int notches, Time minStep, Time maxStep) {
    Time next = view.perDivision;
    for (int n = 0; n < std::abs(notches); ++n) {
        next = notches > 0 ? StepDown(next) : StepUp(next);
    }
    next = std::clamp(next, minStep, maxStep);
    const double factor = static_cast<double>(next) / static_cast<double>(view.perDivision);
    return {.perDivision = next, .left = anchor - Scaled(anchor - view.left, factor)};
}

TimeView Pan(TimeView view, double pixels, float width) {
    view.left -= Scaled(view.span(), pixels / static_cast<double>(width));
    return view;
}

TimeView Clamp(TimeView view, TimeRange record) {
    if (view.span() <= record.length()) {
        view.left = std::clamp(view.left, record.from, record.to - view.span());
    } else {
        view.left = record.from - (view.span() - record.length()) / 2;
    }
    return view;
}

TimeView Fit(TimeRange record) {
    const Time step = StepAtLeast((record.length() + kDivisions - 1) / kDivisions);
    return Clamp({.perDivision = step}, record);
}

TimeView CentredOn(Time perDivision, Time centre) {
    return {.perDivision = perDivision, .left = centre - kDivisions * perDivision / 2};
}

TimeView FirstView(TimeRange record, Time period) {
    return Clamp(CentredOn(StepAtLeast(10 * period), 0), record);
}

std::vector<GridLine> Grid(const TimeView& view, float width, float labelGap) {
    const float perDivisionPx = width / static_cast<float>(kDivisions);
    Time every = 10;
    for (const Time n : {1, 2, 5}) {
        if (static_cast<float>(n) * perDivisionPx >= labelGap) {
            every = n;
            break;
        }
    }
    const Time step = view.perDivision;
    // First whole multiple of the step at or after the left edge.
    Time k = view.left / step;
    if (k * step < view.left) {
        ++k;
    }
    std::vector<GridLine> lines;
    for (; k * step <= view.left + view.span(); ++k) {
        lines.push_back({.time = k * step, .labelled = (k % every + every) % every == 0});
    }
    return lines;
}

std::string GridLabel(Time time) { return FormatTime(time, true); }

std::optional<Time> OffScreenTrigger(const TimeView& view) {
    if (view.left > 0) {
        return -view.left;
    }
    if (view.left + view.span() < 0) {
        return -(view.left + view.span());
    }
    return std::nullopt;
}

std::optional<float> RunTextX(float x0, float x1, float left, float right, float textWidth,
                              float slant) {
    if (x1 <= left || x0 >= right) {
        return std::nullopt;
    }
    const bool cutLeft = x0<left, cutRight = x1> right;
    const float from = cutLeft ? left : x0 + slant;
    const float to = cutRight ? right : x1 - slant;
    if (to - from < textWidth + 2 * kTextPad) {
        return std::nullopt;
    }
    if (cutLeft && !cutRight) {
        return from + kTextPad;
    }
    if (cutRight && !cutLeft) {
        return to - kTextPad - textWidth;
    }
    return (from + to - textWidth) / 2;
}

TimeView ScopeView(const ScopeSettings& settings) {
    return {.perDivision = settings.timebase, .left = settings.delay - settings.timebase};
}

ScopeSettings WithView(ScopeSettings settings, const TimeView& view) {
    settings.timebase = std::clamp(view.perDivision, kTimebases.front(), kTimebases.back());
    const Time tenth = settings.timebase / 10;
    const Time delay = view.left + settings.timebase;
    const Time rounded =
        static_cast<Time>(std::llround(static_cast<double>(delay) / static_cast<double>(tenth))) *
        tenth;
    settings.delay = std::clamp<Time>(rounded, 0, kMaxDelayDivisions * settings.timebase);
    return settings;
}

std::string StatusText(std::optional<Time> cursor, std::optional<Time> marker) {
    std::string text;
    const auto add = [&](const std::string& part) { text += text.empty() ? part : "   " + part; };
    if (cursor) {
        add("cursor " + FormatTime(*cursor, true));
    }
    if (marker) {
        add("M " + FormatTime(*marker, true));
    }
    if (cursor && marker) {
        const Time delta = *cursor - *marker;
        std::string part = "dt " + FormatTime(delta, false);
        if (delta != 0) {
            part += " (" + FormatFrequency(std::abs(delta)) + ")";
        }
        add(part);
    }
    return text;
}

}  // namespace bw
