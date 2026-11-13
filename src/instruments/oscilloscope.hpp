#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "sim/simulator.hpp"

namespace bw {

inline constexpr std::size_t kScopeChannels = 4;
inline constexpr Time kDivisions = 10;
// ps per division: 5 ns … 10 ms in 1-2-5 steps.
inline constexpr std::array<Time, 20> kTimebases{
    5'000,       10'000,        20'000,        50'000,        100'000,
    200'000,     500'000,       1'000'000,     2'000'000,     5'000'000,
    10'000'000,  20'000'000,    50'000'000,    100'000'000,   200'000'000,
    500'000'000, 1'000'000'000, 2'000'000'000, 5'000'000'000, 10'000'000'000};
inline constexpr Time kAutoWait = 50'000'000'000;  // ps; auto waits max(this, one Sweep)
inline constexpr Time kMaxDelayDivisions = 1000;

enum class ScopeEdge : std::uint8_t { Rise, Fall };
enum class ScopeMode : std::uint8_t { Auto, Normal };
enum class SweepKind : std::uint8_t { None, Triggered, Auto };  // None: no Sweep yet

struct ScopeSettings {
    Time timebase = 1'000'000;  // ps per division: 1 µs
    std::size_t source = 0;     // Trigger Channel
    ScopeEdge edge = ScopeEdge::Rise;
    ScopeMode mode = ScopeMode::Auto;
    Time delay = 0;  // ps from the Trigger to the screen's Trigger mark; 0..1000 divisions
    bool operator==(const ScopeSettings&) const = default;
};

struct Sweep {
    std::uint64_t serial = 0;  // 0: none yet
    SweepKind kind = SweepKind::None;
    Time timebase = 0;
    Time delay = 0;  // as configured for this Sweep
    Time start = 0;  // ps since power-on of the left edge, at Trigger + delay - timebase
    // Per Channel: the value at `start` (time = start), then every change in
    // (start, start + 10 divisions]. Empty: no Hook.
    std::array<std::vector<Sample>, kScopeChannels> channels;
};

// Sweep engine over Channel changes in time order. No Simulator, no threads.
class ScopeState {
public:
    void powerOn();  // Channels X, Waiting from 0; the last Sweep is kept
    void configure(const ScopeSettings& settings, Time now);  // drops a Sweep in progress
    // New Net (or none) on the Channel: X until its Sample; unhooked Channels stay empty.
    void clear(std::size_t channel, Time now, bool hooked);
    void change(std::size_t channel, Sample sample);  // may trigger
    void seed(std::size_t channel, Sample sample);    // current value, never triggers
    void advanceTo(Time end);  // completes Sweeps ending <= end, publishes the last one
    const Sweep& sweep() const { return sweep_; }
    SweepKind status() const { return status_; }  // kind of the last Sweep since configure/powerOn

private:
    struct Pending {  // last completed Sweep, materialized by advanceTo
        Time start;
        SweepKind kind;
    };

    Time wait() const;
    void append(std::size_t channel, Sample sample);
    void catchUp(Time time, bool inclusive);  // finish / auto-start Sweeps before `time`
    void startSweep(Time trigger, SweepKind kind);
    void materialize();  // history_ → sweep_
    void prune(Time keepFrom);

    ScopeSettings settings_;
    std::array<std::vector<Sample>, kScopeChannels> history_;
    std::array<LogicValue, kScopeChannels> last_{LogicValue::X, LogicValue::X, LogicValue::X,
                                                 LogicValue::X};
    std::array<bool, kScopeChannels> hooked_{};
    Time now_ = 0;  // latest change or advanceTo
    bool sweeping_ = false;
    Time start_ = 0, end_ = 0;  // Sweep in progress: [start_, end_]
    SweepKind kind_ = SweepKind::None;
    Time waitFrom_ = 0;  // Waiting: Triggers only after this
    std::optional<Pending> pending_;
    SweepKind status_ = SweepKind::None;
    Sweep sweep_;
};

// Headless Oscilloscope (tests). Construct after powerOn().
class Oscilloscope {
public:
    Oscilloscope(Simulator& sim, std::array<std::optional<NetId>, kScopeChannels> channels,
                 const ScopeSettings& settings);
    void update();  // feeds changes since the last update in time order, then advanceTo(now)
    const ScopeState& state() const { return state_; }

private:
    Simulator& sim_;
    std::array<std::optional<NetId>, kScopeChannels> channels_;
    Time fedUntil_ = -1;
    ScopeState state_;
};

std::optional<std::size_t> TimebaseIndex(Time timebase);  // index in kTimebases

}  // namespace bw
