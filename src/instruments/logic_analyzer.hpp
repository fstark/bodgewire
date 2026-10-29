#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sim/logic.hpp"
#include "sim/time.hpp"

namespace bw {

inline constexpr std::size_t kChannels = 8;
inline constexpr std::size_t kDepth = 4096;  // sample points per Capture
// Sample periods, ps: 1 kHz … 100 MHz in 1-2-5 steps.
inline constexpr std::array<Time, 16> kAnalyzerPeriods{
    1'000'000'000, 500'000'000, 200'000'000, 100'000'000, 50'000'000, 20'000'000,
    10'000'000,    5'000'000,   2'000'000,   1'000'000,   500'000,    200'000,
    100'000,       50'000,      20'000,      10'000};

enum class TriggerCondition : std::uint8_t { Any, Low, High, Rise, Fall };
enum class TriggerPosition : std::uint8_t { Early, Middle, Late };  // 10, 50, 90 % of memory

struct AnalyzerSettings {
    Time period = 100'000;  // ps: 10 MHz
    TriggerPosition position = TriggerPosition::Early;
    std::array<TriggerCondition, kChannels> trigger{};  // all Any
    bool operator==(const AnalyzerSettings&) const = default;
};

// One sample point: Channel c is bit c; X where `unknown` has bit c set.
struct ChannelBits {
    std::uint8_t high = 0, unknown = 0xff;
    bool operator==(const ChannelBits&) const = default;
};

struct Capture {
    std::uint64_t serial = 0;                   // 0: none yet; bumped per completed Capture
    Time period = 0;                            // ps between points
    Time triggerTime = 0;                       // ps since power-on of points[trigger]
    std::size_t trigger = 0;                    // index of the Trigger point
    std::size_t first = 0;                      // points before this were not sampled
    std::vector<ChannelBits> points;            // kDepth, oldest first
    std::array<std::string, kChannels> labels;  // "U6.6 1Y"; "" = no Clip
};

enum class AnalyzerPhase : std::uint8_t { Idle, Armed, Filling, Captured };

std::size_t PreTriggerPoints(TriggerPosition position);  // 409, 2048, 3686

// Capture engine over Channel changes in time order. No Simulator, no threads.
class AnalyzerState {
public:
    void powerOn();   // all Channels X, time 0; Armed: waits from point 0
    void powerOff();  // Filling → Armed (Capture in progress dropped)
    void arm(const AnalyzerSettings& settings, std::array<std::string, kChannels> labels,
             Time now);  // memory cleared; first point at the first multiple of period >= now
    void disarm();       // Armed/Filling → Idle; Captured stays
    void change(Time time, std::uint8_t channels, LogicValue value);  // `channels`: bit mask
    void advanceTo(Time end);                                         // samples every point <= end
    AnalyzerPhase phase() const { return phase_; }
    const Capture& capture() const { return capture_; }  // last completed

private:
    void emitUntil(Time limit, bool inclusive);
    void emit(std::uint64_t count);        // `count` points of current_, trigger and memory
    void write(std::uint64_t count);       // `count` points of current_ into ring_
    bool triggers(ChannelBits now) const;  // vs previous_, per settings_.trigger
    void freeze();                         // ring → capture_, Captured

    AnalyzerPhase phase_ = AnalyzerPhase::Idle;
    AnalyzerSettings settings_;
    std::array<std::string, kChannels> labels_;
    ChannelBits current_;                  // state after every change so far
    std::optional<ChannelBits> previous_;  // last emitted point; none right after arm
    Time next_ = 0;                        // ps of the next point to emit
    Time armedAt_ = 0;                     // ps of point 0 since arming
    std::vector<ChannelBits> ring_ = std::vector<ChannelBits>(kDepth);
    std::uint64_t emitted_ = 0;       // points since arming
    std::uint64_t triggerPoint_ = 0;  // emitted_ index of the Trigger point
    std::uint64_t remaining_ = 0;     // Filling: points still to sample
    Capture capture_;
};

std::optional<TriggerCondition> ParseTrigger(std::string_view text);  // "0" "1" "r" "f"

}  // namespace bw
