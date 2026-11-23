#include "runner/channel_nets.hpp"

#include "runner/runner.hpp"

namespace bw {

void Rewatch(Runner& runner, std::span<const std::optional<NetId>> before,
             std::span<const std::optional<NetId>> after) {
    for (std::size_t c = 0; c < after.size(); ++c) {
        if (after[c] != before[c] && after[c]) {
            runner.watch(*after[c]);
        }
    }
    for (std::size_t c = 0; c < before.size(); ++c) {
        if (after[c] != before[c] && before[c]) {
            runner.unwatch(*before[c]);
        }
    }
}

}  // namespace bw
