#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "sim/ids.hpp"

namespace bw {

class Runner;

// UI thread: watches each Net a Channel gains from `before` to `after`, then unwatches each Net a
// Channel lost (watches are counted, so a Net moving between Channels stays watched). The
// Oscilloscope's and the Logic Analyzer's Consumers.
void Rewatch(Runner& runner, std::span<const std::optional<NetId>> before,
             std::span<const std::optional<NetId>> after);

// Devices thread: the distinct Nets of an instrument's Channels, each with the Channels it feeds
// (bit c: Channel c), for nets() and for sending a Sample to its Channels.
template <class Mask>
class ChannelNets {
public:
    void adopt(std::span<const std::optional<NetId>> channels) {
        nets_.clear();
        masks_.clear();
        for (std::size_t c = 0; c < channels.size(); ++c) {
            if (!channels[c]) {
                continue;
            }
            const auto bit = static_cast<Mask>(Mask{1} << c);
            const auto found = std::ranges::find(nets_, *channels[c]);
            if (found == nets_.end()) {
                nets_.push_back(*channels[c]);
                masks_.push_back(bit);
            } else {
                masks_[static_cast<std::size_t>(found - nets_.begin())] |= bit;
            }
        }
    }
    std::span<const NetId> nets() const { return nets_; }
    Mask maskOf(NetId net) const {  // 0: no Channel
        const auto found = std::ranges::find(nets_, net);
        return found == nets_.end() ? Mask{0}
                                    : masks_[static_cast<std::size_t>(found - nets_.begin())];
    }

private:
    std::vector<NetId> nets_;  // distinct
    std::vector<Mask> masks_;  // Channels per nets_ entry
};

}  // namespace bw
