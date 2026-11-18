#include "view/bench_places.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace bw {

namespace {

constexpr float kKeyboardTop = kBenchMargin + kTvFirstWidth * kTvAspect + kBenchMargin;

float Distance(Extent a, Extent b) {
    return std::abs(a.width - b.width) + std::abs(a.height - b.height);
}

}  // namespace

Place DefaultPlace(BenchObject object, Extent bench) {
    const float m = kBenchMargin;
    const float w = bench.width, h = bench.height;
    switch (object) {
    case BenchObject::Tv:
        return {w - m, m, 1, 0};
    case BenchObject::Keyboard:
        return {w - m, kKeyboardTop, 1, 0};
    case BenchObject::Knob:
        return {m, m, 0, 0};
    case BenchObject::Analyzer:
        return {m, h - m, 0, 1};
    case BenchObject::Scope:
        return {w / 2, h - m, 0.5f, 1};
    case BenchObject::Meter:
        return {w - m, h - m, 1, 1};
    case BenchObject::Manual:
        return {m, m + kKnobBox.height + m, 0, 0};
    case BenchObject::Brief:
        return {w / 2, m, 0.5f, 0};
    case BenchObject::Debug:
        return {w / 2, h - m, 0.5f, 1};
    }
    return {};
}

Extent FirstSize(BenchObject object, Extent bench, float aspect, int count) {
    switch (object) {
    case BenchObject::Tv:
        return {kTvFirstWidth, kTvFirstWidth * kTvAspect};
    case BenchObject::Knob:
        return {kKnobBox.width * static_cast<float>(count), kKnobBox.height};
    case BenchObject::Keyboard: {
        const float room = bench.height - kKeyboardTop - kBenchMargin;
        const float width =
            std::min({kKeyboardFirstWidth, room / aspect, bench.width - 2 * kBenchMargin});
        return {width, width * aspect};
    }
    case BenchObject::Manual:
        return {600, 500};
    case BenchObject::Brief:
        return {340, 120};
    case BenchObject::Debug:
        return {420, 140};
    default:
        assert(false && "an instrument: NormalSize");
        return {};
    }
}

Extent NormalSize(BenchObject instrument) {
    switch (instrument) {
    case BenchObject::Analyzer:
        return {940, 440};
    case BenchObject::Scope:
        return {680, 460};
    case BenchObject::Meter:
        return {320, 200};
    default:
        assert(false && "not an instrument");
        return {};
    }
}

Extent MinimalSize(BenchObject instrument) {
    switch (instrument) {
    case BenchObject::Analyzer:
        return {560, 260};
    case BenchObject::Scope:
        return {420, 300};
    case BenchObject::Meter:
        return {240, 150};
    default:
        assert(false && "not an instrument");
        return {};
    }
}

Extent NextPreset(Extent current, Extent minimal, Extent normal) {
    return Distance(current, minimal) > Distance(current, normal) ? minimal : normal;
}

std::optional<Area> BesidePlace(Area object, Area main, std::span<const Area> displays) {
    // Each side, each display: the place nearest to just beside the main window that lies on the
    // display and still clears the main window.
    for (const bool right : {true, false}) {
        for (const Area& d : displays) {
            const float x = right ? std::max(main.x + main.width + kBenchMargin, d.x)
                                  : std::min(main.x - kBenchMargin - object.width,
                                             d.x + d.width - object.width);
            if (x < d.x || x + object.width > d.x + d.width || object.height > d.height) {
                continue;
            }
            const float y = std::clamp(object.y, d.y, d.y + d.height - object.height);
            return Area{x, y, object.width, object.height};
        }
    }
    return std::nullopt;
}

Extent FitBench(Extent size, Extent bench) {
    const float scale = std::min({1.0f, bench.width / size.width, bench.height / size.height});
    return {size.width * scale, size.height * scale};
}

}  // namespace bw
