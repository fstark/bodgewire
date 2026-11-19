#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sim/logic.hpp"
#include "sim/simulator.hpp"
#include "sim/time.hpp"

namespace bw {

inline constexpr std::size_t kChannels = 64;   // both sides of the BW-80's data bus, its address
inline constexpr std::size_t kDepth = 65'536;  // sample points per Capture
using ChannelMask = std::uint64_t;             // bit c: Channel c
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
    ChannelMask high = 0, unknown = ~ChannelMask{0};
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

std::size_t PreTriggerPoints(TriggerPosition position);  // 6553, 32768, 58982

// Capture engine over Channel changes in time order. No Simulator, no threads.
class AnalyzerState {
public:
    void powerOn();   // all Channels X, time 0; Armed: waits from point 0
    void powerOff();  // Filling → Armed (Capture in progress dropped)
    void arm(const AnalyzerSettings& settings, std::array<std::string, kChannels> labels,
             Time now);  // memory cleared; first point at the first multiple of period >= now
    void disarm();       // Armed/Filling → Idle; Captured stays
    void change(Time time, ChannelMask channels, LogicValue value);
    void advanceTo(Time end);  // samples every point <= end
    AnalyzerPhase phase() const { return phase_; }
    const Capture& capture() const { return capture_; }  // last completed

private:
    void emitUntil(Time limit, bool inclusive);
    void emit(std::uint64_t count);        // `count` points of current_, trigger and memory
    void write(std::uint64_t count);       // `count` points of current_ into ring_
    bool triggers(ChannelBits now) const;  // vs previous_, per masks_
    void freeze();                         // ring → capture_, Captured

    struct TriggerMasks {  // Channels per condition, from settings_.trigger at arm()
        ChannelMask low = 0, high = 0, rise = 0, fall = 0;
    };

    AnalyzerPhase phase_ = AnalyzerPhase::Idle;
    AnalyzerSettings settings_;
    TriggerMasks masks_;
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

// Headless Logic Analyzer on a Simulator (tests). Construct after powerOn().
class LogicAnalyzer {
public:
    LogicAnalyzer(Simulator& sim, std::array<std::optional<NetId>, kChannels> channels);
    void arm(const AnalyzerSettings& settings);  // from sim.now()
    void update();  // feeds changes recorded since the last update, in time order
    const AnalyzerState& state() const { return state_; }

private:
    Simulator& sim_;
    std::array<std::optional<NetId>, kChannels> channels_;
    Time fedUntil_ = -1;
    AnalyzerState state_;
};

std::optional<TriggerCondition> ParseTrigger(std::string_view text);  // "0" "1" "r" "f"

// A bus pattern over `width` Channels, bit 0 first in the result: hex digits and x/X (don't
// care), at most ceil(width / 4) digits, fewer padded with leading 0s. Refused: any other
// character, too many digits, a 1 above the width. Empty: all Any.
std::optional<std::vector<TriggerCondition>> ParseBusPattern(std::string_view pattern,
                                                             std::size_t width);
// Back from conditions (bit 0 first): a nibble of levels is its hex digit, all Any is x, anything
// else (partly Any, or an edge) is ?.
std::string FormatBusPattern(std::span<const TriggerCondition> conditions);

struct NameStem {
    std::string stem;     // the pin name with its last digit run replaced by '#': "A#", "#Q"
    std::uint32_t index;  // that digit run's value
    bool operator==(const NameStem&) const = default;
};
std::optional<NameStem> StemOf(std::string_view pin);  // nullopt: no digit in the name

}  // namespace bw
