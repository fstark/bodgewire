#include "view/scope_reading.hpp"

#include <cmath>
#include <format>
#include <vector>

#include "instruments/volts.hpp"
#include "view/time_text.hpp"

namespace bw {

namespace {

enum class Level : std::uint8_t { High, Low, Floating, Unknown };

Level LevelOf(const Sample& sample) {
    if (sample.floating) {
        return Level::Floating;
    }
    switch (sample.value) {
    case LogicValue::One:
        return Level::High;
    case LogicValue::Zero:
        return Level::Low;
    default:
        return Level::Unknown;
    }
}

struct Change {
    Time time;
    Level level;
    const Sample* sample;
};

}  // namespace

std::string BandReading(std::span<const Sample> samples, Time from, Time to,
                        std::optional<double> rail) {
    if (samples.empty()) {
        return "";
    }
    if (rail) {
        return std::format("{:.1f} V", *rail);
    }
    // The level at `from`, then each change of level up to `to`.
    std::vector<Change> changes;
    for (const Sample& sample : samples) {
        if (sample.time > to) {
            break;
        }
        const Level level = LevelOf(sample);
        if (sample.time <= from) {  // the value at `from`: the last Sample up to it
            changes.assign(1, {from, level, &sample});
        } else if (changes.empty() || level != changes.back().level) {
            changes.push_back({sample.time, level, &sample});
        }
    }
    if (changes.empty()) {
        return "";
    }
    std::vector<Time> rises;
    for (std::size_t i = 1; i < changes.size(); ++i) {
        if (changes[i].level == Level::High) {
            rises.push_back(changes[i].time);
        }
    }
    if (rises.size() >= 2) {
        const Time span = rises.back() - rises.front();
        Time high = 0;
        for (std::size_t i = 0; i < changes.size(); ++i) {
            const Time begin = std::max(changes[i].time, rises.front());
            const Time end =
                std::min(i + 1 < changes.size() ? changes[i + 1].time : to, rises.back());
            if (changes[i].level == Level::High && end > begin) {
                high += end - begin;
            }
        }
        const Time period = span / static_cast<Time>(rises.size() - 1);
        const auto share =
            std::llround(100.0 * static_cast<double>(high) / static_cast<double>(span));
        return std::format("{} · {} %", FormatFrequency(period), share);
    }
    for (std::size_t i = 1; i + 1 < changes.size(); ++i) {
        const Level level = changes[i].level;
        if (level == Level::High || level == Level::Low) {
            return std::format("{} {}", level == Level::High ? "high" : "low",
                               FormatTime(changes[i + 1].time - changes[i].time, false));
        }
    }
    switch (changes.back().level) {
    case Level::High:
        return "H";
    case Level::Low:
        return "L";
    case Level::Floating:
        return std::format("Z {:.1f} V", LogicVolts(*changes.back().sample));
    case Level::Unknown:
        return "X";
    }
    return "";
}

}  // namespace bw
