#include "view/neon_sign.hpp"

#include "sim/rng.hpp"

namespace bw {

namespace {

constexpr double kStrikeEnd = 0.4;  // s: lit from here
constexpr double kQuietUntil = 4;   // s: no event before
constexpr double kSlot = 8;         // s: at most one event in each
constexpr double kLetterChance = 0.25;
constexpr double kBrownoutChance = 0.12;
constexpr std::uint64_t kSeed = 0x6e656f6e;  // "neon"

double Uniform(Rng& rng) { return static_cast<double>(rng.next() >> 11) * 0x1p-53; }

double Between(Rng& rng, double low, double high) { return low + (high - low) * Uniform(rng); }

float Level(Rng& rng, double low, double high) {
    return static_cast<float>(Between(rng, low, high));
}

// The tubes striking at the start: dark, a dim blip, nearly dark, lit.
float Strike(double seconds) {
    if (seconds < 0.15) {
        return 0;
    }
    if (seconds < 0.21) {
        return 0.55f;
    }
    return seconds < kStrikeEnd ? 0.08f : 1;
}

// A letter failing `at` s into its slot. Every draw from `rng` is made in the same order whatever
// `at`, so each moment of the slot sees the same event. It ends by 7.5 s.
NeonSign FailingLetter(Rng& rng, double at, int letters) {
    NeonSign sign{.letter = static_cast<int>(rng.next() % static_cast<std::uint64_t>(letters))};
    double t = Between(rng, 0, kSlot - 3.5);
    if (at < t) {
        return {};
    }
    // It stutters: dim and lit by turns, 40 to 120 ms each.
    const double stutterEnd = t + Between(rng, 0.3, 0.6);
    for (bool low = true; t < stutterEnd; low = !low) {
        const double end = t + Between(rng, 0.04, 0.12);
        const float level = low ? Level(rng, 0, 0.25) : Level(rng, 0.6, 1);
        if (at < end) {
            sign.letterBrightness = level;
            return sign;
        }
        t = end;
    }
    // Dark, or one half lit, 1 to 2 s.
    const double darkEnd = t + Between(rng, 1, 2);
    const double kind = Uniform(rng);
    if (at < darkEnd) {
        if (kind < 0.35) {
            sign.lit = kind < 0.175 ? NeonPart::LeftHalf : NeonPart::RightHalf;
            sign.letterBrightness = 0.85f;
        } else {
            sign.letterBrightness = 0;
        }
        return sign;
    }
    // It strikes back: a blip, dark again, lit.
    const double blipEnd = darkEnd + 0.05;
    if (at < blipEnd) {
        sign.letterBrightness = 0.7f;
        return sign;
    }
    if (at < blipEnd + Between(rng, 0.1, 0.2)) {
        sign.letterBrightness = 0;
        return sign;
    }
    return {};
}

// The supply dipping once or twice, 100 to 300 ms each, to 35 to 55 %. It starts after 1 s and
// ends by 7.9 s, so two slots' dips are at least 1 s apart.
NeonSign Brownout(Rng& rng, double at) {
    double t = Between(rng, 1, kSlot - 1);
    const int dips = rng.nextBit() ? 2 : 1;
    for (int i = 0; i < dips; ++i) {
        const double end = t + Between(rng, 0.1, 0.3);
        const float level = Level(rng, 0.35, 0.55);
        if (at < t) {
            return {};
        }
        if (at < end) {
            return {.sign = level};
        }
        t = end + Between(rng, 0.15, 0.3);
    }
    return {};
}

}  // namespace

NeonSign NeonSignAt(double seconds, int letters, std::uint64_t run) {
    if (seconds < kStrikeEnd) {
        return {.sign = Strike(seconds)};
    }
    if (seconds < kQuietUntil || letters <= 0) {
        return {};
    }
    const double since = seconds - kQuietUntil;
    const auto slot = static_cast<std::uint64_t>(since / kSlot);
    const double at = since - static_cast<double>(slot) * kSlot;
    Rng rng(kSeed + run * 0x9e3779b97f4a7c15ULL + slot);
    const double roll = Uniform(rng);
    if (roll < kLetterChance) {
        return FailingLetter(rng, at, letters);
    }
    if (roll < kLetterChance + kBrownoutChance) {
        return Brownout(rng, at);
    }
    return {};
}

}  // namespace bw
