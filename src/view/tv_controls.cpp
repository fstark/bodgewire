#include "view/tv_controls.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "text/text.hpp"

namespace bw {

namespace {

// "N%" or "N.N%", 0 to 100, as a turn 0..1.
std::optional<double> Turn(std::string_view text) {
    return ParsePercent(text).transform([](double percent) { return percent / 100; });
}

std::optional<TvKnob> KnobNamed(std::string_view name) {
    if (name == "hpos") {
        return TvKnob::HPos;
    }
    if (name == "vpos") {
        return TvKnob::VPos;
    }
    if (name == "bright") {
        return TvKnob::Bright;
    }
    if (name == "contrast") {
        return TvKnob::Contrast;
    }
    return std::nullopt;
}

}  // namespace

const char* Label(TvKnob knob) {
    switch (knob) {
    case TvKnob::HPos:
        return "H-POS";
    case TvKnob::VPos:
        return "V-POS";
    case TvKnob::Bright:
        return "BRIGHT";
    case TvKnob::Contrast:
        return "CONTRAST";
    }
    return "";
}

double Mark(TvKnob knob) {
    switch (knob) {
    case TvKnob::HPos:
    case TvKnob::VPos:
        return 0.5;
    case TvKnob::Bright:
        return 0.25;
    case TvKnob::Contrast:
        return 0.75;
    }
    return 0.5;
}

void TvControls::set(TvKnob knob, double turn) {
    turns_[static_cast<std::size_t>(knob)] = std::clamp(turn, 0.0, 1.0);
}

PictureWindow TvControls::window(int lines) const {
    if (underscan_) {
        return {};
    }
    const int bottom =
        lines >= static_cast<int>(kWindowTop) + 100 ? std::min(lines + 1, kTvRows) : kTvRows;
    return {kWindowLeft, kWindowRight, kWindowTop, static_cast<float>(bottom)};
}

PictureShift TvControls::shift() const {
    return {static_cast<float>((turn(TvKnob::HPos) - 0.5) / 2),
            static_cast<float>(-(turn(TvKnob::VPos) - 0.5) / 2)};
}

TvTone TvControls::tone() const {
    return {static_cast<float>((turn(TvKnob::Bright) - Mark(TvKnob::Bright)) * 0.4),
            static_cast<float>(turn(TvKnob::Contrast) / Mark(TvKnob::Contrast))};
}

std::uint8_t Grey(float cell, TvTone tone) {
    return static_cast<std::uint8_t>(
        std::lround(std::clamp(tone.black + tone.gain * cell, 0.0f, 1.0f) * 255.0f));
}

std::optional<ScreenFraction> ParseLoupe(std::string_view text) {
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }
    const auto x = Turn(text.substr(0, colon));
    const auto y = Turn(text.substr(colon + 1));
    if (!x || !y) {
        return std::nullopt;
    }
    return ScreenFraction{*x, *y};
}

std::optional<TvControls> ParseTvSettings(std::string_view list, TvControls start) {
    TvControls controls = start;
    for (const std::string_view item : Split(list, ',')) {
        if (item == "off") {
            controls.setOn(false);
        } else if (item == "underscan") {
            controls.setUnderscan(true);
        } else {
            const std::size_t colon = item.find(':');
            if (colon == std::string_view::npos) {
                return std::nullopt;
            }
            const std::optional<TvKnob> knob = KnobNamed(item.substr(0, colon));
            const std::optional<double> turn = Turn(item.substr(colon + 1));
            if (!knob || !turn) {
                return std::nullopt;
            }
            controls.set(*knob, *turn);
        }
    }
    return controls;
}

}  // namespace bw
