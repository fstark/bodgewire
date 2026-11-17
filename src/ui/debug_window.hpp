#pragma once

#include <cstdint>
#include <string_view>

#include "session/session.hpp"

namespace bw {

struct DebugStats {
    double simSeconds, speedRatio, ringWaitShare, uiMilliseconds;
    std::string_view lock, state;
};

// The developer's window (`--debug`, P6d): the seed of the next power-on, the run's numbers, the
// keys sent to the first keyboard.
void DrawDebug(bool& open, const Session& session, const DebugStats& stats, std::uint64_t& seed);

}  // namespace bw
