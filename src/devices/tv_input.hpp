#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "devices/tv.hpp"
#include "sim/ids.hpp"
#include "sim/simulator.hpp"

namespace bw {

// Plugs a Tv into the Nets of a VIDEO connector (two) or a MONITOR (three) of a Simulator.
// Construct after powerOn() (it clears recordings). Never trims: whoever runs the loop calls
// sim.trimRecordings() after updating every consumer.
class TvInput {
public:
    TvInput(Simulator& sim, NetId video, NetId syncN);
    TvInput(Simulator& sim, SeparateDrives drives);
    void update();  // feeds the changes recorded since the last update, then up to sim.now()
    const Tv& tv() const { return tv_; }

private:
    Simulator& sim_;
    std::vector<NetId> nets_;  // VIDEO, SYNC_N; or VIDEO_N, HDRIVE, VDRIVE
    Time fedUntil_ = -1;       // Samples up to here already fed
    Tv tv_;
};

}  // namespace bw
