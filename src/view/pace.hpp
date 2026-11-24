#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

#include "sim/time.hpp"

namespace bw {

// What the run switch shows of the speed (simulated over wall time, the last second's).
struct Pace {
    bool behind = false;  // the slider red
    double shown = 0;     // the speed printed, held 0.5 s so its last digit does not flicker
};

// Behind once the speed ratio has stayed under 0.9 for 2 s; caught up as soon as it is over 0.95,
// so a ratio wavering at the bar does not flicker. Not running (off, paused): not behind, the 2 s
// waited again, the next speed shown at once.
class PaceWatch {
public:
    using Clock = std::chrono::steady_clock;
    Pace update(bool running, double ratio, Clock::time_point now);

private:
    Pace pace_;
    std::optional<Clock::time_point> under_;  // since when under the bar
    std::optional<Clock::time_point> shownAt_;
};

// How well a Machine keeps up, from its headroom: the launcher's bar colour (2026-11-24).
enum class Playability : std::uint8_t { Playable, Slow, TooSlow };

// How fast a Machine could run while the launcher runs it at 1×: simulated time over the wall time
// its simulation thread spent running (pacing sleeps excluded), the last second's. Its band goes
// down under 0.9× and 0.27×, back up over 1× and 0.3×, so a Machine at a limit does not flicker;
// the number shown is held 0.5 s, as the run switch's.
class Headroom {
public:
    using Clock = std::chrono::steady_clock;
    // `runNs`: RunnerStatus::runNs. Either going back (powered on again): measured anew.
    void update(Time simTime, std::int64_t runNs, Clock::time_point now);
    void reset();                                           // another Machine: measured anew
    std::optional<double> shown() const { return shown_; }  // none until a quarter second's work
    Playability band() const { return band_; }

private:
    struct Point {
        Clock::time_point wall;
        Time simTime;
        std::int64_t runNs;
    };
    std::deque<Point> history_;
    std::optional<double> shown_;
    std::optional<Clock::time_point> shownAt_;
    Playability band_ = Playability::Playable;
};

// The headroom in a sentence under the launcher's paragraph: "On this computer it runs at full
// speed: it could go 13× as fast as the real one." (Playable), "On this computer it runs at 34% of
// the real one's speed." (else, at most 99%).
std::string HeadroomText(double ratio, Playability band);

// A speed ratio printed: "0.24×", "3.40×", "42×".
std::string SpeedText(double ratio);

}  // namespace bw
