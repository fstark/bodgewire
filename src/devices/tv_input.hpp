#pragma once

#include <cstddef>
#include <cstdint>

#include "devices/tv.hpp"
#include "sim/ids.hpp"
#include "sim/simulator.hpp"

namespace bw {

// Plugs a Tv into two Nets of a Simulator. Construct after powerOn() (it clears recordings).
class TvInput {
public:
    enum class History : std::uint8_t { Trim, Keep };  // Keep when a VCD needs the Samples

    TvInput(Simulator& sim, NetId video, NetId syncN, History history);
    void update();  // feeds the changes recorded since the last update, then up to sim.now()
    const Tv& tv() const { return tv_; }

private:
    Simulator& sim_;
    NetId video_, syncN_;
    History history_;
    std::size_t videoFed_ = 0, syncNFed_ = 0;  // Samples already fed, per Net
    Tv tv_;
};

}  // namespace bw
