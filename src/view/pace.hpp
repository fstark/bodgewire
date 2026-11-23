#pragma once

#include <chrono>
#include <optional>
#include <string>

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

// A speed ratio printed: "0.24×", "3.40×", "42×".
std::string SpeedText(double ratio);

}  // namespace bw
